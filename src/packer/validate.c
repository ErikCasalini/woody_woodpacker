#include "validate.h"
#include <elf.h>
#include <stddef.h>
#include <sys/mman.h>

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

static error_t	check_phdr(const mapped_file_t *file, uint64_t page_size)
{
	Elf64_Ehdr	*header;
	Elf64_Phdr	*phdrs;
	size_t		phdr_size;
	size_t		file_sz;
	Elf64_Phdr	*pt_interp = NULL;
	uintptr_t	prev_vaddr = 0;
	uintptr_t	segment_end;
	int			pt_load_num = 0;

	if (!file || file->data == MAP_FAILED)
		return (unexpected_error);

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
		if (segment_end > UINT64_MAX - (page_size - 1))
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

error_t	check_elf_validity(const mapped_file_t *file, uint64_t page_size)
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

	error = check_phdr(file, page_size);
	if (error != ok)
		return (error);

	return (ok);
}
