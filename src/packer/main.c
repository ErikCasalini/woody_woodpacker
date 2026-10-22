#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <elf.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include "../ft/libft.h"
#include "../common/payload.h"

#ifndef LOWEST_VADDR
# define LOWEST_VADDR 0x10000
#endif

#ifndef HIGHEST_VADDR
# define HIGHEST_VADDR 0x00007fffffffefff
#endif

typedef struct mapping_range_s
{
	uintptr_t	start;
	uintptr_t	end;
} mapping_range_t;

typedef struct elf_layout_s
{
	Elf64_Ehdr	header;
	Elf64_Phdr	phdrs_segment;
	Elf64_Phdr	stub_segment;
	Elf64_Phdr	payload_segment;
	Elf64_Phdr	gnu_stack_segment;
} elf_layout_t;

typedef struct mapped_file_s
{
	void		*data;
	struct stat st;
} mapped_file_t;

typedef enum error_e
{
	ok,
	too_short,
	set_uid,
	not_a_file,
	bad_ident,
	bad_type,
	bad_machine,
	bad_version,
	bad_flags,
	bad_ehsize,
	bad_phentsize,
	bad_shoff,
	no_stub_section,
	no_phdr,
	no_ptload,
	bad_phdr,
	segment_overflow,
	bad_pt_load_order,
	bad_pt_load_align,
	bad_p_align,
	bad_interp_name,
	bad_page_sz,
	et_exec_collide,
	write_error,
	unexpected_error,
	bad_open,
	bad_mmap,
	bad_munmap,
	bad_fstat,
	bad_close
} error_t;

static void	print_error(error_t error)
{
	switch (error)
	{
		case ok:
			break;
		case too_short:
			fprintf(stderr, "Error: file is too short for its ELF headers or segment contents\n");
			break;
		case set_uid:
			fprintf(stderr, "Error: set-user-ID and set-group-ID files are not supported\n");
			break;
		case not_a_file:
			fprintf(stderr, "Error: target is not a regular file\n");
			break;
		case bad_ident:
			fprintf(stderr, "Error: invalid or unsupported ELF identification\n");
			break;
		case bad_type:
			fprintf(stderr, "Error: ELF type must be ET_EXEC or ET_DYN\n");
			break;
		case bad_machine:
			fprintf(stderr, "Error: only x86-64 ELF files are supported\n");
			break;
		case bad_version:
			fprintf(stderr, "Error: unsupported ELF version\n");
			break;
		case bad_flags:
			fprintf(stderr, "Error: invalid ELF flags\n");
			break;
		case bad_ehsize:
			fprintf(stderr, "Error: invalid ELF header size\n");
			break;
		case bad_phentsize:
			fprintf(stderr, "Error: invalid program header entry size\n");
			break;
		case bad_shoff:
			fprintf(stderr, "Error: invalid section header table offset\n");
			break;
		case no_stub_section:
			fprintf(stderr, "Error: .stub section not found\n");
			break;
		case no_phdr:
			fprintf(stderr, "Error: ELF program header table is missing\n");
			break;
		case no_ptload:
			fprintf(stderr, "Error: ELF has no PT_LOADs\n");
			break;
		case bad_phdr:
			fprintf(stderr, "Error: invalid ELF program header\n");
			break;
		case segment_overflow:
			fprintf(stderr, "Error: segment size or address overflows during payload layout\n");
			break;
		case bad_pt_load_order:
			fprintf(stderr, "Error: PT_LOAD segments are not ordered by virtual address\n");
			break;
		case bad_pt_load_align:
			fprintf(stderr, "Error: PT_LOAD virtual address and file offset have incompatible alignment\n");
			break;
		case bad_p_align:
			fprintf(stderr, "Error: invalid segment alignment\n");
			break;
		case bad_interp_name:
			fprintf(stderr, "Error: invalid ELF interpreter name\n");
			break;
		case bad_page_sz:
			fprintf(stderr, "Error: unable to determine system page size\n");
			break;
		case et_exec_collide:
			fprintf(stderr, "Error: no address range available for the stub without overlapping the target ELF\n");
			break;
		case write_error:
			fprintf(stderr, "Error: unable to write output file\n");
			break;
		case bad_open:
			perror("Error: open()");
			break;
		case bad_mmap:
			perror("Error: mmap()");
			break;
		case bad_munmap:
			perror("Error: munmap()");
			break;
		case bad_fstat:
			perror("Error: fstat()");
			break;
		case bad_close:
			perror("Error: close()");
			break;
		default:
			fprintf(stderr, "Error: unexpected error (%d)\n", error);
	}
}

static error_t	check_file_metadata(struct stat target_st)
{
	if (!S_ISREG(target_st.st_mode))
    	return (not_a_file);
	// A creuser
	if (target_st.st_mode & (S_ISUID | S_ISGID))
	    return (set_uid);

	return (ok);
}

static error_t	check_header(const mapped_file_t *file)
{
	Elf64_Ehdr	*header;

	if (!file || file->data == MAP_FAILED)
		return (unexpected_error);

	if ((size_t)file->st.st_size < sizeof(Elf64_Ehdr))
		return (too_short);

	header = (Elf64_Ehdr*)file->data;

	if (header->e_ident[EI_MAG0] != ELFMAG0)
		return (bad_ident);
	if (header->e_ident[EI_MAG1] != ELFMAG1)
		return (bad_ident);
	if (header->e_ident[EI_MAG2] != ELFMAG2)
		return (bad_ident);
	if (header->e_ident[EI_MAG3] != ELFMAG3)
		return (bad_ident);
	if (header->e_ident[EI_CLASS] != ELFCLASS64)
		return (bad_ident);
	if (header->e_ident[EI_DATA] != ELFDATA2LSB)
		return (bad_ident);
	if (header->e_ident[EI_VERSION] != EV_CURRENT)
		return (bad_ident);
	if (header->e_ident[EI_OSABI] != ELFOSABI_SYSV
		&& header->e_ident[EI_OSABI] != ELFOSABI_GNU)
		return (bad_ident);
	if (header->e_ident[EI_ABIVERSION] != 0)
		return (bad_ident);
	if (header->e_type != ET_EXEC && header->e_type != ET_DYN)
		return (bad_type);
	if (header->e_machine != EM_X86_64)
		return (bad_machine);
	if (header->e_version != EV_CURRENT)
		return (bad_version);
	if (header->e_flags != 0)
		return (bad_flags);
	if (header->e_ehsize != sizeof(Elf64_Ehdr))
		return (bad_ehsize);
	if (header->e_phentsize != sizeof(Elf64_Phdr))
		return (bad_phentsize);

	return (ok);
}

static error_t	check_interp_name(const Elf64_Ehdr *header, const Elf64_Phdr *pt_interp)
{
	char	*interp_name;

	if (!header || !pt_interp)
		return (bad_interp_name);

	if (pt_interp->p_filesz <= 1)
		return (bad_interp_name);

	interp_name = (char*)((unsigned char*)header + pt_interp->p_offset);

	if (interp_name[pt_interp->p_filesz - 1] != '\0')
		return (bad_interp_name);

	return (ok);
}

static error_t	check_phdr(const mapped_file_t *file)
{
	Elf64_Ehdr	*header;
	Elf64_Phdr	*phdrs;
	size_t		phdr_size;
	size_t		file_sz;
	Elf64_Phdr	*pt_interp = NULL;
	uintptr_t	prev_vaddr = 0;
	uintptr_t	segment_end;
	int64_t		page_size;
	int			pt_load_num = 0;

	if (!file || file->data == MAP_FAILED)
		return (unexpected_error);

	page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0)
	    return (bad_page_sz);

	header = (Elf64_Ehdr*)file->data;
	file_sz = (size_t)file->st.st_size;

	if (!header->e_phoff || !header->e_phnum)
		return (no_phdr);

	/* Checks if file has enought space for all its pretended phdrs */
	phdr_size = (size_t)header->e_phnum * sizeof(Elf64_Phdr);
	if (header->e_phoff > file_sz
		|| phdr_size > file_sz - header->e_phoff)
		return (too_short);

	phdrs = (Elf64_Phdr*)((unsigned char*)header + header->e_phoff);

	for (Elf64_Half i = 0; i < header->e_phnum; i++) {
		if (phdrs[i].p_type == PT_INTERP && pt_interp == NULL)
			pt_interp = &phdrs[i];

		if (phdrs[i].p_type == PT_NULL)
			continue;
		/* Checks if file has enought space for phdr content */
		if (phdrs[i].p_offset > file_sz
			|| phdrs[i].p_filesz > file_sz - phdrs[i].p_offset)
			return (too_short);

		if (phdrs[i].p_type != PT_LOAD)
			continue;
		pt_load_num++;

		if (phdrs[i].p_filesz > phdrs[i].p_memsz)
			return (bad_phdr);

		/* Ensure segment_end wont overflow */
		if (phdrs[i].p_memsz > UINT64_MAX - phdrs[i].p_vaddr)
			return (bad_phdr);
		segment_end = phdrs[i].p_vaddr + phdrs[i].p_memsz;
		/* Ensure the segment round up on page_size wont overflow */
		if (segment_end > UINT64_MAX - ((uint64_t)page_size - 1))
			return (bad_phdr);

		if (phdrs[i].p_vaddr < prev_vaddr)
			return (bad_pt_load_order);
		prev_vaddr = phdrs[i].p_vaddr;

		if (phdrs[i].p_align > 1 &&
			(phdrs[i].p_align & (phdrs[i].p_align - 1)) != 0
		)
			return (bad_p_align);

		if (phdrs[i].p_align > 1 &&
			(phdrs[i].p_vaddr % phdrs[i].p_align) != (phdrs[i].p_offset % phdrs[i].p_align)
		)
			return (bad_pt_load_align);
	}

	if (!pt_load_num)
		return (no_ptload);

	if (pt_interp)
		return (check_interp_name(header, pt_interp));

	return (ok);
}

static error_t	check_elf_validity(const mapped_file_t *file)
{
	error_t	error;

	if (!file || file->data == MAP_FAILED)
		return (unexpected_error);

	error = check_file_metadata(file->st);
	if (error != ok)
		return (error);

	error = check_header(file);
	if (error != ok)
		return (error);

	error = check_phdr(file);
	if (error != ok)
		return (error);

	return (ok);
}

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
	header->e_phnum = 4;
	header->e_shentsize = 0;
	header->e_shnum = 0;
	header->e_shstrndx = 0;
}

static uintptr_t	round_to_page_up(uintptr_t value, uint64_t page_size)
{
	uintptr_t tmp;

	tmp = value % page_size;
	if (tmp == 0)
		return (value);

	return (value + page_size - tmp);
}

static uintptr_t	round_to_page_down(uintptr_t value, uint64_t page_size)
{
	return (value - (value % page_size));
}

static mapping_range_t	get_mapping_range(const Elf64_Phdr *phdr, Elf64_Half phnum, uint64_t page_size)
{
	mapping_range_t	range = {0};
	uintptr_t	segment_end;
	Elf64_Half	i = 0;

	if (!phdr)
		return (range);

	while (i < phnum) {
		if (phdr[i].p_type == PT_LOAD) {
			range.start = round_to_page_down(phdr[i].p_vaddr, page_size);
			range.end = round_to_page_up(
				phdr[i].p_vaddr + phdr[i].p_memsz,
				page_size
			);
			break;
		}
		i++;
	}

	i = 0;

	while (i < phnum) {
		if (phdr[i].p_type == PT_LOAD) {
			segment_end = round_to_page_up(
				phdr[i].p_vaddr + phdr[i].p_memsz,
				page_size
			);
			if (segment_end > range.end)
				range.end = segment_end;
		}
		i++;
	}

	return (range);
}

static intptr_t	select_output_base_addr(const Elf64_Ehdr *target_file_data, size_t output_memsz, int64_t page_size)
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

/* This function searches for a "name" section in a mapped file.
 The function must be used only on the tursted packer file produced
 by our build. For the moment, no bounds or overflow checks are performed.
 Malformed metadata may cause undefined behavior.
 If no "name" section is found, NULL is returned. */
static const Elf64_Shdr	*find_packer_section(const mapped_file_t *packer, const char *name)
{
	Elf64_Ehdr	*header;
	Elf64_Shdr	*shdrs;
	char		*section_names;
	size_t		name_len;

	if (!packer || !name)
		return (NULL);

	name_len = ft_strlen(name);
	header = packer->data;
	if (!header
		|| !header->e_shoff
		|| name_len < 1)
		return (NULL);

	shdrs = (Elf64_Shdr*)((unsigned char*)header + header->e_shoff);
	section_names = (char*)header + shdrs[header->e_shstrndx].sh_offset;

	for (Elf64_Half i = 0; i < header->e_shnum; i++) {
		if (ft_strncmp(section_names + shdrs[i].sh_name,
					name,
					name_len + 1
				) == 0
			) {
			return (&shdrs[i]);
		}
	}

	return (NULL);
}

static void	init_phdrs_segment(Elf64_Phdr *phdrs_segment, Elf64_Off file_off, intptr_t vaddr)
{
	phdrs_segment->p_type = PT_LOAD;
	phdrs_segment->p_flags = PF_R;
	phdrs_segment->p_offset = file_off;
	phdrs_segment->p_vaddr = vaddr;
	phdrs_segment->p_paddr = vaddr;
	/* We know that packed ELF will contain exactly 4 segments */
	phdrs_segment->p_filesz = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) * 4;
	phdrs_segment->p_memsz = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) * 4;
	phdrs_segment->p_align = 0x1000;
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
	stub_segment->p_align = 0x1000;
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
	payload_segment->p_align = 0x1000;

	return (ok);
}

/* Requires a non-NULL, zero-initialized destination and a validated target ELF. */
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

static error_t	init_output_headers(elf_layout_t *output, const mapped_file_t *target, const Elf64_Shdr *packer_stub_section)
{
	off_t		file_off;
	size_t		prev_mem_end;
	intptr_t	vaddr;
	intptr_t	base_addr;
	int64_t		alignment;
	size_t		output_memsz;
	error_t		error;

	if (!output || !target || !target->data
		|| target->data == MAP_FAILED || !packer_stub_section)
		return (unexpected_error);

	alignment = sysconf(_SC_PAGESIZE);
	if (alignment == -1)
		return (bad_page_sz);

	init_header(&output->header, target->data);

	file_off = 0;
	vaddr = 0;
	init_phdrs_segment(&output->phdrs_segment, file_off, vaddr);

	prev_mem_end = output->phdrs_segment.p_vaddr + output->phdrs_segment.p_memsz;
	file_off = output->phdrs_segment.p_offset + output->phdrs_segment.p_filesz;
	vaddr = round_to_page_up(prev_mem_end, alignment) + (file_off % alignment);
	init_stub_segment(&output->stub_segment, packer_stub_section, file_off, vaddr);

	prev_mem_end = output->stub_segment.p_vaddr + output->stub_segment.p_memsz;
	file_off = output->stub_segment.p_offset + output->stub_segment.p_filesz;
	vaddr = round_to_page_up(prev_mem_end, alignment) + (file_off % alignment);
	error = init_payload_segment(&output->payload_segment,
		(size_t)target->st.st_size, file_off, vaddr, alignment);
	if (error != ok)
		return (error);

	init_gnu_stack_segment(&output->gnu_stack_segment, target->data);

	/* We calculate the total page space that our output ELF will require
	(before the payload execution) */
	output_memsz = (size_t)round_to_page_up(
								output->payload_segment.p_vaddr +
								output->payload_segment.p_memsz,
								alignment
							);

	base_addr = select_output_base_addr(target->data, output_memsz, alignment);
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

#ifdef PACKER_DEBUG
static void	print_output_segment(const char *name, const Elf64_Phdr *segment)
{
	printf("\n%s\n", name);
	printf("  p_type:   %" PRIu32 "\n", segment->p_type);
	printf("  p_flags:  0x%08" PRIx32 " (%c%c%c)\n", segment->p_flags,
		(segment->p_flags & PF_R) ? 'R' : '-',
		(segment->p_flags & PF_W) ? 'W' : '-',
		(segment->p_flags & PF_X) ? 'X' : '-');
	printf("  p_offset: 0x%016" PRIx64 "\n", segment->p_offset);
	printf("  p_vaddr:  0x%016" PRIx64 "\n", segment->p_vaddr);
	printf("  p_paddr:  0x%016" PRIx64 "\n", segment->p_paddr);
	printf("  p_filesz: 0x%016" PRIx64 "\n", segment->p_filesz);
	printf("  p_memsz:  0x%016" PRIx64 "\n", segment->p_memsz);
	printf("  p_align:  0x%016" PRIx64 "\n", segment->p_align);
}

static void	print_output_layout(const elf_layout_t *output)
{
	Elf64_Ehdr	*header = &output->header;

	printf("ELF header\n");
	printf("  e_ident:    ");
	for (size_t i = 0; i < EI_NIDENT; i++)
		printf(" %02x", (unsigned int)header->e_ident[i]);
	printf("\n");
	printf("  e_type:      %u\n", (unsigned int)header->e_type);
	printf("  e_machine:   %u\n", (unsigned int)header->e_machine);
	printf("  e_version:   %" PRIu32 "\n", header->e_version);
	printf("  e_entry:     0x%016" PRIx64 "\n", header->e_entry);
	printf("  e_phoff:     0x%016" PRIx64 "\n", header->e_phoff);
	printf("  e_shoff:     0x%016" PRIx64 "\n", header->e_shoff);
	printf("  e_flags:     0x%08" PRIx32 "\n", header->e_flags);
	printf("  e_ehsize:    0x%04x\n", (unsigned int)header->e_ehsize);
	printf("  e_phentsize: 0x%04x\n", (unsigned int)header->e_phentsize);
	printf("  e_phnum:     %u\n", (unsigned int)header->e_phnum);
	printf("  e_shentsize: 0x%04x\n", (unsigned int)header->e_shentsize);
	printf("  e_shnum:     %u\n", (unsigned int)header->e_shnum);
	printf("  e_shstrndx:  %u\n", (unsigned int)header->e_shstrndx);
	print_output_segment("phdrs_segment (PT_LOAD)", &output->phdrs_segment);
	print_output_segment("stub_segment (PT_LOAD)", &output->stub_segment);
	print_output_segment("payload_segment (PT_LOAD)", &output->payload_segment);
	print_output_segment("gnu_stack_segment (GNU_STACK)", &output->gnu_stack_segment);
}
#endif

static error_t	fill_output_elf(int						output_fd,
								const elf_layout_t		*output,
								const Elf64_Shdr		*packer_stub_section,
								const Elf64_Ehdr		*packer_file_data,
								const Elf64_Ehdr		*target_file_data,
								const payload_header_t	*payload_header)
{
	void	*start_stub_section;
	ssize_t	written;

	start_stub_section = (const unsigned char*)packer_file_data + packer_stub_section->sh_offset;

	written = write(output_fd, &output->header, sizeof(output->header));
	if (written != (ssize_t)sizeof(output->header))
		return (write_error);

	written = write(output_fd, &output->phdrs_segment, sizeof(output->phdrs_segment));
	if (written != (ssize_t)sizeof(output->phdrs_segment))
		return (write_error);

	written = write(output_fd, &output->stub_segment, sizeof(output->stub_segment));
	if (written != (ssize_t)sizeof(output->stub_segment))
		return (write_error);

	written = write(output_fd, &output->payload_segment, sizeof(output->payload_segment));
	if (written != (ssize_t)sizeof(output->payload_segment))
		return (write_error);

	written = write(output_fd, &output->gnu_stack_segment, sizeof(output->gnu_stack_segment));
	if (written != (ssize_t)sizeof(output->gnu_stack_segment))
		return (write_error);

	written = write(output_fd, start_stub_section, packer_stub_section->sh_size);
	if (written != (ssize_t)packer_stub_section->sh_size)
		return (write_error);

	written = write(output_fd, payload_header, sizeof(payload_header_t));
	if (written != (ssize_t)sizeof(payload_header_t))
		return (write_error);

	written = write(output_fd, target_file_data, payload_header->decompressed_size);
	if (written != (ssize_t)payload_header->decompressed_size)
		return (write_error);

	return (ok);
}

static error_t	map_file(const char *path, mapped_file_t *file)
{
	error_t	error = ok;
	int		fd;
	void	*data;
	int		errno_backup;

	if (!path || !file)
		return (unexpected_error);

	file->data = MAP_FAILED;
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (bad_open);

	if (fstat(fd, &file->st) < 0) {
		error = bad_fstat;
		goto cleanup;
	}

	data = mmap(NULL, file->st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (data == MAP_FAILED) {
		error = bad_mmap;
		goto cleanup;
	}

	if (close(fd) < 0) {
		errno_backup = errno;
		munmap(data, file->st.st_size);
		errno = errno_backup;
		return (bad_close);
	}

	file->data = data;
	return (ok);

cleanup:
	errno_backup = errno;
	close(fd);
	errno = errno_backup;
	return (error);
}

static int	create_output_file(const char *path, const struct stat *target_st)
{
	int			fd;
	int			errno_backup;
	struct stat	output_st;
	/* We avoid O_TRUNC in case target and output are
	the same file */
	fd = open(path, O_CREAT | O_WRONLY, 0755);
	if (fd < 0)
		return (-1);

	if (fstat(fd, &output_st) < 0)
		goto cleanup;
	/* We ensure target and output files are not the same */
	if (target_st->st_dev == output_st.st_dev
		&& target_st->st_ino == output_st.st_ino) {
		errno = EINVAL;
		goto cleanup;
	}
	/* We need to manually truncate a potentially already existent
	output file */
	if (ftruncate(fd, 0) < 0)
		goto cleanup;
	return (fd);

cleanup:
	errno_backup = errno;
	close(fd);
	errno = errno_backup;
	return (-1);
}

static void	init_payload_header(payload_header_t *payload_header, const mapped_file_t *target)
{
	ft_memcpy(payload_header->magic, PAYLOAD_MAGIC, sizeof(PAYLOAD_MAGIC));
	payload_header->compressed_size	= (size_t)target->st.st_size;
	payload_header->decompressed_size = (size_t)target->st.st_size;
	payload_header->offset = sizeof(payload_header_t);
}

/*
   "packer": represents this program/file
   "target": represents the input ELF that will be packed
   "output": represents the final packed ELF
   "payload": represents the "target" ELF file embded inside of "output"
   "stub": represents the code/data that will run inside of "output"
    in order to find/decrypt/load/run "payload", and that is originally
	embded inside of "packer"
*/
int main(int argc, char *argv[])
{
	mapped_file_t 		target = {.data = MAP_FAILED};
	mapped_file_t 		packer = {.data = MAP_FAILED};
	int					output_fd = -1;
	payload_header_t	payload_header = {0};
	elf_layout_t		output_layout = {0};
	Elf64_Shdr			*packer_stub_section;
	error_t				error;
	int					status = 1;

	if (argc < 2)
		return (fprintf(stderr, "Specify a filename\n"), 1);

	error = map_file(argv[1], &target);
	if (error != ok) {
		print_error(error);
		goto cleanup;
	}

	error = check_elf_validity(&target);
	if (error != ok) {
		print_error(error);
		goto cleanup;
	}

	error = map_file("/proc/self/exe", &packer);
	if (error != ok) {
		print_error(error);
		goto cleanup;
	}

	packer_stub_section = find_packer_section(&packer, ".stub");
	if (packer_stub_section == NULL) {
		print_error(no_stub_section);
		goto cleanup;
	}

	error = init_output_headers(
				&output_layout,
				&target,
				packer_stub_section
			);
	if (error != ok) {
		print_error(error);
		goto cleanup;
	}

	init_payload_header(&payload_header, &target);

#ifdef PACKER_DEBUG
	print_output_layout(&output_layout);
#endif

	output_fd = create_output_file("woody", &target.st);
	if (output_fd < 0) {
		perror("Error: create_output_file()");
		goto cleanup;
	}

	error = fill_output_elf(
				output_fd,
				&output_layout,
				packer_stub_section,
				packer.data,
				target.data,
				&payload_header
			);
	if (error != ok) {
		print_error(error);
		if (syscall(SYS_unlink, "woody") < 0)
			perror("Error: unlink()");
		goto cleanup;
	}

	/* Success */
	status = 0;

cleanup:
	if (output_fd >= 0 && close(output_fd) < 0) {
		perror("Error: close()");
		status = 1;
	}
	if (target.data != MAP_FAILED && munmap(target.data, target.st.st_size) < 0)
		perror("Error: unmapping file");
	if (packer.data != MAP_FAILED && munmap(packer.data, packer.st.st_size) < 0)
		perror("Error: unmapping file");
	if (status == 0)
		printf("ELF successuflly packed!\n");

	return (status);
}
