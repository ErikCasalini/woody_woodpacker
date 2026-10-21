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

#ifndef LOWEST_VADDR
# define LOWEST_VADDR 0x10000
#endif

#ifndef HIGHEST_VADDR
# define HIGHEST_VADDR 0x00007fffffffefff
#endif

typedef struct payload_header_s
{
	unsigned char	magic[8];
	Elf64_Xword		compressed_size;
	Elf64_Xword		decompressed_size;
	Elf64_Off		offset;				/* From the beginning of this segment */
	Elf64_Xword		key;
} payload_header_t;

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
	bad_phdr,
	bad_pt_load_order,
	bad_pt_load_align,
	bad_p_align,
	bad_interp_name,
	bad_page_sz,
	et_exec_collide,
	write_error,
	unexpected_error
} error_t;

void	print_error(error_t error)
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
			fprintf(stderr, "Error: stub section not found\n");
			break;
		case no_phdr:
			fprintf(stderr, "Error: ELF program header table is missing\n");
			break;
		case bad_phdr:
			fprintf(stderr, "Error: invalid ELF program header\n");
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
		default:
			fprintf(stderr, "Error: unexpected error (%d)\n", error);
	}
}

error_t	check_file_metadata(struct stat target_st)
{
	if (!S_ISREG(target_st.st_mode))
    	return (not_a_file);
	// A creuser
	if (target_st.st_mode & (S_ISUID | S_ISGID))
	    return (set_uid);

	return (ok);
}

error_t check_header(Elf64_Ehdr *target_header, size_t target_file_size)
{
	if (!target_header)
		return (unexpected_error);

	if (target_file_size < sizeof(Elf64_Ehdr))
		return (too_short);

	if (target_header->e_ident[EI_MAG0] != ELFMAG0)
		return (bad_ident);
	if (target_header->e_ident[EI_MAG1] != ELFMAG1)
		return (bad_ident);
	if (target_header->e_ident[EI_MAG2] != ELFMAG2)
		return (bad_ident);
	if (target_header->e_ident[EI_MAG3] != ELFMAG3)
		return (bad_ident);
	if (target_header->e_ident[EI_CLASS] != ELFCLASS64)
		return (bad_ident);
	if (target_header->e_ident[EI_DATA] != ELFDATA2LSB)
		return (bad_ident);
	if (target_header->e_ident[EI_VERSION] != EV_CURRENT)
		return (bad_ident);
	if (target_header->e_ident[EI_OSABI] != ELFOSABI_SYSV
		&& target_header->e_ident[EI_OSABI] != ELFOSABI_GNU)
		return (bad_ident);
	if (target_header->e_ident[EI_ABIVERSION] != 0)
		return (bad_ident);
	if (target_header->e_type != ET_EXEC && target_header->e_type != ET_DYN)
		return (bad_type);
	if (target_header->e_machine != EM_X86_64)
		return (bad_machine);
	if (target_header->e_version != EV_CURRENT)
		return (bad_version);
	if (target_header->e_flags != 0)
		return (bad_flags);
	if (target_header->e_ehsize != sizeof(Elf64_Ehdr))
		return (bad_ehsize);
	if (target_header->e_phentsize != sizeof(Elf64_Phdr))
		return (bad_phentsize);

	return (ok);
}

int	check_interp_name(Elf64_Ehdr *target_header, Elf64_Phdr *pt_interp)
{
	char *interp_name;

	if (!target_header || !pt_interp)
		return (-1);

	if (pt_interp->p_filesz <= 1)
		return (1);

	interp_name = (char*)((unsigned char*)target_header + pt_interp->p_offset);

	if (interp_name[pt_interp->p_filesz] != '\0')
		return (-1);

	return (0);
}

error_t	check_phdr(Elf64_Ehdr *target_header, size_t target_file_size)
{
	Elf64_Phdr	*target_program_headers;
	Elf64_Phdr	*pt_interp = NULL;
	uintptr_t	prev_vaddr = 0;
	size_t		farthest_offset = 0;

	if (!target_header->e_phoff || !target_header->e_phnum)
		return (no_phdr);

	/* Checks if file has enought space for all its pretended phdrs */
	if (target_header->e_phoff + (target_header->e_phnum * target_header->e_phentsize) > target_file_size)
		return (too_short);

	target_program_headers = (Elf64_Phdr*)((unsigned char*)target_header + target_header->e_phoff);

	for (Elf64_Half i = 0; i < target_header->e_phnum; i++) {
		if (target_program_headers[i].p_type == PT_INTERP && pt_interp == NULL)
			pt_interp = &target_program_headers[i];

		if (target_program_headers[i].p_offset + target_program_headers[i].p_filesz > farthest_offset)
			farthest_offset = target_program_headers[i].p_offset + target_program_headers[i].p_filesz;

		if (target_program_headers[i].p_type != PT_LOAD)
			continue;

		if (target_program_headers[i].p_vaddr < prev_vaddr)
			return (bad_pt_load_order);

		if (target_program_headers[i].p_align > 1 &&
			(target_program_headers[i].p_align & (target_program_headers[i].p_align - 1)) != 0
		)
			return (bad_p_align);

		if (target_program_headers[i].p_align > 1 &&
			(target_program_headers[i].p_vaddr % target_program_headers[i].p_align) != (target_program_headers[i].p_offset % target_program_headers[i].p_align)
		)
			return (bad_pt_load_align);
	}

	/* Checks if file has enought space for all the content of its phdrs */
	if (farthest_offset > target_file_size)
		return (too_short);

	if (check_interp_name(target_header, pt_interp) < 0)
				return (bad_interp_name);

	return (ok);
}

error_t	check_elf_validity(Elf64_Ehdr *target_header, struct stat target_st)
{
	error_t	error;

	if (!target_header)
		return (unexpected_error);

	error = check_file_metadata(target_st);
	if (error)
		return (error);

	error = check_header(target_header, target_st.st_size);
	if (error)
		return (error);

	error = check_phdr(target_header, target_st.st_size);
	if (error)
		return (error);

	return (ok);
}

void	init_header(Elf64_Ehdr *header, Elf64_Ehdr *target_header)
{
	if (!header || !target_header)
		return;

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

uintptr_t	round_to_page_up(uintptr_t value, uint64_t page_size)
{
	uintptr_t tmp;

	tmp = value % page_size;
	if (tmp == 0)
		return (value);

	return (value + page_size - tmp);
}

uintptr_t	round_to_page_down(uintptr_t value, uint64_t page_size)
{
	return (value - (value % page_size));
}

mapping_range_t	get_mapping_range(Elf64_Phdr *phdr, Elf64_Half phnum, uint64_t page_size)
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

intptr_t	select_output_base_addr(Elf64_Ehdr *target_file_data, size_t output_memsz, int64_t page_size)
{
	Elf64_Phdr		*target_phdrs;
	mapping_range_t	target_load_range;

	if (!target_file_data || target_file_data->e_type == ET_DYN)
		return (0x0);

	/* If original ELF is ET_EXEC, we must chose absolute addresses
	that will not collide with any of its PT_LOAD. Thus we calculate
	the load range of the entire ELF, and we place the stub PT_LOADs
	rigth after or right before, depending of available addresses */
	target_phdrs = (Elf64_Phdr*)((unsigned char*)target_file_data + target_file_data->e_phoff);
	target_load_range = get_mapping_range(target_phdrs, target_file_data->e_phnum, page_size);

	/* We assume that the lowest possible vaddr for a process is 0x10000
	and the highest is 0x00007fffffffefff (47 bits) */
	if ((LOWEST_VADDR + output_memsz) < target_load_range.start)
		return ((intptr_t)round_to_page_down(
					target_load_range.start - output_memsz,
					page_size
				));
	if ((HIGHEST_VADDR - output_memsz) > target_load_range.end)
		return (target_load_range.end);

	return (-1);
}

Elf64_Shdr	*find_section(Elf64_Ehdr *packer_header, char *name)
{
	Elf64_Shdr	*shdr_table;
	char		*section_names;
	Elf64_Shdr	*section_header = NULL;

	if (!packer_header || !name)
		return (NULL);

	shdr_table = (Elf64_Shdr*)((unsigned char*)packer_header + packer_header->e_shoff);
	if (!shdr_table ||
		!name ||
		ft_strlen(name) < 1)
		return (NULL);
	section_names = (char*)packer_header + shdr_table[packer_header->e_shstrndx].sh_offset;

	for (Elf64_Half i = 0; i < packer_header->e_shnum; i++) {
		if (ft_strncmp(section_names + shdr_table[i].sh_name,
				name,
				ft_strlen(name)
			) == 0
		) {
			section_header = &shdr_table[i];
			break;
		}
	}

	return (section_header);
}

void	init_phdrs_segment(Elf64_Phdr *phdrs_segment, Elf64_Off file_off, intptr_t vaddr)
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

void	init_stub_segment(Elf64_Phdr *stub_segment, Elf64_Shdr *packer_stub_section, Elf64_Off file_offset, intptr_t vaddr)
{
	if (!stub_segment)
		return;

	stub_segment->p_type = PT_LOAD;
	stub_segment->p_flags = PF_R | PF_X;
	stub_segment->p_offset = file_offset;
	stub_segment->p_vaddr = vaddr;
	stub_segment->p_paddr = vaddr;
	stub_segment->p_filesz = packer_stub_section->sh_size;
	stub_segment->p_memsz = packer_stub_section->sh_size;
	stub_segment->p_align = 0x1000;
}

void	init_payload_segment(Elf64_Phdr *payload_segment, size_t payload_size, Elf64_Off file_offset, intptr_t vaddr)
{
	if (!payload_segment)
		return;

	payload_segment->p_type = PT_LOAD;
	payload_segment->p_flags = PF_R | PF_W;
	payload_segment->p_offset = file_offset;
	payload_segment->p_vaddr = vaddr;
	payload_segment->p_paddr = vaddr;
	payload_segment->p_filesz = payload_size;
	payload_segment->p_memsz = payload_size;
	payload_segment->p_align = 0x1000;
}

/* We assume that ELF validity check has already been ensured */
void	init_gnu_stack_segment(Elf64_Phdr *gnu_stack_segment, Elf64_Ehdr *target_file_data)
{
	Elf64_Half	target_phnum;
	Elf64_Phdr	*target_phdr;

	if (!gnu_stack_segment || !target_file_data)
		return;

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

/* We assume that the beginning of both sections is alligned
on a virtual memory address. This inply that no extra memory
padding will be needed, and the in-memory layout of those sections
will be the same as their in-file layout */
error_t	init_output_headers(elf_layout_t *output, Elf64_Ehdr *target_file_data, size_t target_file_size, Elf64_Shdr *packer_stub_section)
{
	off_t			file_off;
	size_t			prev_mem_end;
	intptr_t		vaddr;
	intptr_t		base_addr;
	int64_t			alignment;
	size_t			output_memsz;

	if (!output || !target_file_data || !packer_stub_section)
		return (unexpected_error);

	alignment = sysconf(_SC_PAGESIZE);
	if (alignment == -1)
		return (bad_page_sz);

	init_header(&output->header, target_file_data);

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
	init_payload_segment(&output->payload_segment, sizeof(payload_header_t) + target_file_size, file_off, vaddr);

	init_gnu_stack_segment(&output->gnu_stack_segment, target_file_data);

	/* We calculate the total page space that our output ELF will require */
	output_memsz = (size_t)round_to_page_up(
								output->payload_segment.p_vaddr +
								output->payload_segment.p_memsz,
								alignment
							);

	base_addr = select_output_base_addr(target_file_data, output_memsz, alignment);
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

void	print_output_layout(const elf_layout_t *output)
{
	const Elf64_Ehdr	*header = &output->header;

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

error_t	fill_output_elf(int					output_fd,
						elf_layout_t		*output,
						Elf64_Shdr			*packer_stub_section,
						Elf64_Ehdr			*packer_file_data,
						Elf64_Ehdr			*target_file_data,
						payload_header_t	*encrypt_header)
{
	void	*start_stub_section;
	ssize_t	written;

	start_stub_section = (unsigned char*)packer_file_data + packer_stub_section->sh_offset;

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

	written = write(output_fd, encrypt_header, sizeof(payload_header_t));
	if (written != (ssize_t)sizeof(payload_header_t))
		return (write_error);

	written = write(output_fd, target_file_data, encrypt_header->decompressed_size);
	if (written != (ssize_t)encrypt_header->decompressed_size)
		return (write_error);

	return (ok);
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
	int					target_fd = -1;
	int					packer_fd = -1;
	int					output_fd = -1;
	int					status = 1;
	payload_header_t	encrypt_header = {0};
	void				*target_file_data = MAP_FAILED;
	void				*packer_file_data = MAP_FAILED;
	struct stat			target_st;
	struct stat			packer_st;
	elf_layout_t		output_layout = {0};
	Elf64_Shdr			*packer_stub_section;
	error_t				error;

	if (argc < 2)
		return (fprintf(stderr, "Specify a filename\n"), 1);

	/* OPEN FILE */
	target_fd = open(argv[1], O_RDONLY);
	if (target_fd < 0) {
		perror("Error: open()");
		goto cleanup;
	}

	if (fstat(target_fd, &target_st) < 0) {
		perror("Error: fstat()");
		goto cleanup;
	}

	target_file_data = mmap(NULL, target_st.st_size, PROT_READ, MAP_PRIVATE, target_fd, 0);
	if (target_file_data == MAP_FAILED) {
		perror("Error: mmap()");
		goto cleanup;
	}

	error = check_elf_validity(target_file_data, target_st);
	if (error != ok) {
		print_error(error);
		goto cleanup;
	}

	/* OPEN OURSELVES */
	packer_fd = open(argv[0], O_RDONLY);
	if (packer_fd < 0) {
		perror("Error: open()");
		goto cleanup;
	}

	if (fstat(packer_fd, &packer_st) < 0) {
		perror("Error: fstat()");
		goto cleanup;
	}

	packer_file_data = mmap(NULL, packer_st.st_size, PROT_READ, MAP_PRIVATE, packer_fd, 0);
	if (packer_file_data == MAP_FAILED) {
		perror("Error: mmap()");
		goto cleanup;
	}

	packer_stub_section = find_section(packer_file_data, ".stub");
	if (packer_stub_section == NULL) {
		fprintf(stderr, "Error: .stub.text section not found\n");
		goto cleanup;
	}

	error = init_output_headers(
				&output_layout,
				target_file_data,
				target_st.st_size,
				packer_stub_section
			);
	if (error != ok) {
		print_error(error);
		goto cleanup;
	}

	ft_memcpy(encrypt_header.magic, "WOODY", sizeof("WOODY"));
	encrypt_header.compressed_size	= target_st.st_size;
	encrypt_header.decompressed_size = target_st.st_size;
	encrypt_header.offset = sizeof(payload_header_t);

	// print_output_layout(&output_layout);

	/* CREATE OUTPUT ELF */
	output_fd = open("woody", O_CREAT | O_WRONLY | O_TRUNC, 0755);
	if (output_fd < 0) {
		perror("Error: open()");
		goto cleanup;
	}

	error = fill_output_elf(
				output_fd,
				&output_layout,
				packer_stub_section,
				packer_file_data,
				target_file_data,
				&encrypt_header
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
	if (packer_file_data != MAP_FAILED)
		munmap(packer_file_data, packer_st.st_size);
	if (packer_fd >= 0)
		close(packer_fd);
	if (target_file_data != MAP_FAILED)
		munmap(target_file_data, target_st.st_size);
	if (target_fd >= 0)
		close(target_fd);
	if (status == 0)
		printf("ELF successuflly packed!\n");

	return (status);
}
