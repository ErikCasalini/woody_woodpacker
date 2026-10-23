#include "layout.h"
#include <sys/mman.h>
#include "../common/elf_range.h"
#include "../common/page.h"
#include "../common/payload.h"
#include "../ft/libft.h"

#define OUTPUT_PHNUM 4
#define OUTPUT_ALIGN 0x1000

#ifndef LOWEST_VADDR
# define LOWEST_VADDR 0x10000
#endif

#ifndef HIGHEST_VADDR
# define HIGHEST_VADDR 0x00007fffffffefff
#endif

static void	init_header(Elf64_Ehdr *header, const Elf64_Ehdr *target_header)
{
	ft_memcpy(header->e_ident, target_header->e_ident, sizeof(header->e_ident));

	header->e_type = target_header->e_type;
	header->e_machine = EM_X86_64;
	header->e_version = EV_CURRENT;
	header->e_flags = 0;
	header->e_ehsize = sizeof(Elf64_Ehdr);
	header->e_phentsize = sizeof(Elf64_Phdr);
	header->e_phoff = header->e_ehsize;
	header->e_phnum = OUTPUT_PHNUM;
	header->e_shentsize = 0;
	header->e_shnum = 0;
	header->e_shstrndx = 0;
}

static intptr_t	select_output_base_addr(const Elf64_Ehdr *target_file_data, size_t output_memsz, uint64_t page_size)
{
	Elf64_Phdr		*target_phdrs;
	mapping_range_t	target_load_range;

	if (!target_file_data || target_file_data->e_type == ET_DYN)
		return (0);

	/* If original ELF is ET_EXEC, we must chose absolute addresses
	that will not collide with any of its PT_LOAD. Thus we calculate
	the load range of the entire ELF, and we place the stub PT_LOADs
	rigth before or right after it, depending of available addresses */
	target_phdrs = (Elf64_Phdr*)((unsigned char*)target_file_data + target_file_data->e_phoff);
	target_load_range = get_mapping_range(target_phdrs, target_file_data->e_phnum, page_size);

	/* We assume that the lowest possible vaddr for a process is 0x10000
	and the highest is 0x00007fffffffefff (47 bits) */
	if (target_load_range.start > LOWEST_VADDR
		&& output_memsz < target_load_range.start - LOWEST_VADDR)
		return ((intptr_t)round_to_page_down(
					target_load_range.start - output_memsz,
					page_size
				));
	if (target_load_range.end < HIGHEST_VADDR
		&& output_memsz < HIGHEST_VADDR - target_load_range.end)
		return (target_load_range.end);

	return (-1);
}

static void	init_phdrs_segment(Elf64_Phdr *phdrs_segment, Elf64_Off file_off, intptr_t vaddr)
{
	phdrs_segment->p_type = PT_LOAD;
	phdrs_segment->p_flags = PF_R;
	phdrs_segment->p_offset = file_off;
	phdrs_segment->p_vaddr = vaddr;
	phdrs_segment->p_paddr = vaddr;
	phdrs_segment->p_filesz = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) * OUTPUT_PHNUM;
	phdrs_segment->p_memsz = phdrs_segment->p_filesz;
	phdrs_segment->p_align = OUTPUT_ALIGN;
}

static void	init_stub_segment(Elf64_Phdr *stub_segment, const Elf64_Shdr *packer_stub_section, Elf64_Off file_off, intptr_t vaddr)
{
	stub_segment->p_type = PT_LOAD;
	stub_segment->p_flags = PF_R | PF_X;
	stub_segment->p_offset = file_off;
	stub_segment->p_vaddr = vaddr;
	stub_segment->p_paddr = vaddr;
	stub_segment->p_filesz = packer_stub_section->sh_size;
	stub_segment->p_memsz = packer_stub_section->sh_size;
	stub_segment->p_align = OUTPUT_ALIGN;
}

static error_t	init_payload_segment(Elf64_Phdr *payload_segment, size_t target_size, Elf64_Off file_off, uintptr_t vaddr, uint64_t alignment)
{
	size_t		payload_size;
	uintptr_t	payload_end;

	if (!payload_segment || alignment == 0)
		return (unexpected_error);

	/* Ensure adding the payload header to the file size cannot overflow */
	if (target_size > SIZE_MAX - sizeof(payload_header_t))
		return (segment_overflow);
	payload_size = sizeof(payload_header_t) + target_size;
	/* Ensure the payload end address cannot overflow */
	if (payload_size > UINTPTR_MAX - vaddr)
		return (segment_overflow);
	payload_end = vaddr + payload_size;
	/* Ensure rounding the payload end up to alignment cannot overflow */
	if (payload_end > UINTPTR_MAX - (alignment - 1))
		return (segment_overflow);

	payload_segment->p_type = PT_LOAD;
	payload_segment->p_flags = PF_R | PF_W;
	payload_segment->p_offset = file_off;
	payload_segment->p_vaddr = vaddr;
	payload_segment->p_paddr = vaddr;
	payload_segment->p_filesz = payload_size;
	payload_segment->p_memsz = payload_size;
	payload_segment->p_align = OUTPUT_ALIGN;

	return (ok);
}

static void	init_gnu_stack_segment(Elf64_Phdr *gnu_stack_segment, const Elf64_Ehdr *target_file_data)
{
	Elf64_Half	target_phnum;
	Elf64_Phdr	*target_phdr;

	gnu_stack_segment->p_type = PT_GNU_STACK;
	gnu_stack_segment->p_flags = PF_R | PF_W;
	gnu_stack_segment->p_memsz = 0x0;

	target_phnum = target_file_data->e_phnum;
	target_phdr = (Elf64_Phdr*)((unsigned char*)target_file_data + target_file_data->e_phoff);

	for (Elf64_Half i = 0; i < target_phnum; i++) {
		if (target_phdr[i].p_type == PT_GNU_STACK) {
			gnu_stack_segment->p_flags = target_phdr[i].p_flags;
			gnu_stack_segment->p_memsz = target_phdr[i].p_memsz;
			break;
		}
	}
}

error_t	init_output_headers(elf_layout_t *output, const mapped_file_t *target, const Elf64_Shdr *packer_stub_section, uint64_t page_size)
{
	off_t		file_off;
	size_t		prev_mem_end;
	intptr_t	vaddr;
	intptr_t	base_addr;
	size_t		output_memsz;
	error_t		error;

	if (!output || !target || !target->data
		|| target->data == MAP_FAILED || !packer_stub_section)
		return (unexpected_error);

	init_header(&output->header, target->data);

	file_off = 0;
	vaddr = 0;
	init_phdrs_segment(&output->phdrs_segment, file_off, vaddr);

	prev_mem_end = output->phdrs_segment.p_vaddr + output->phdrs_segment.p_memsz;
	file_off = output->phdrs_segment.p_offset + output->phdrs_segment.p_filesz;
	vaddr = round_to_page_up(prev_mem_end, page_size) + (file_off % page_size);
	init_stub_segment(&output->stub_segment, packer_stub_section, file_off, vaddr);

	prev_mem_end = output->stub_segment.p_vaddr + output->stub_segment.p_memsz;
	file_off = output->stub_segment.p_offset + output->stub_segment.p_filesz;
	vaddr = round_to_page_up(prev_mem_end, page_size) + (file_off % page_size);
	error = init_payload_segment(&output->payload_segment,
		(size_t)target->st.st_size, file_off, vaddr, page_size);
	if (error != ok)
		return (error);

	init_gnu_stack_segment(&output->gnu_stack_segment, target->data);

	/* We calculate the total page space that our output ELF will require
	(before the payload execution) */
	output_memsz = (size_t)round_to_page_up(
								output->payload_segment.p_vaddr +
								output->payload_segment.p_memsz,
								page_size
							);

	base_addr = select_output_base_addr(target->data, output_memsz, page_size);
	if (base_addr < 0)
		return (et_exec_collide);

	output->phdrs_segment.p_vaddr += base_addr;
	output->stub_segment.p_vaddr += base_addr;
	output->payload_segment.p_vaddr += base_addr;

	output->phdrs_segment.p_paddr += base_addr;
	output->stub_segment.p_paddr += base_addr;
	output->payload_segment.p_paddr += base_addr;

	/* We assume that entry code is at the very beginning
	of .stub section */
	output->header.e_entry = output->stub_segment.p_vaddr;

	return (ok);
}
