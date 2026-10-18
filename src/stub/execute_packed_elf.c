#include <stdint.h>
#include <stddef.h>
#include <elf.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "syscalls/syscalls.h"
#include "../ft/libft.h"
#include "jump_to_with_stack.h"

#define STDIN 0
#define STDOUT 1
#define STDERR 2
#define EXIT_FAILURE 1
#define STRINGS_LEN 468
#define PATH_MAX 4096

typedef enum str_code_e
{
	ok,
	bad_stack,
	bad_auxv,
	blob_not_found,
	bad_e_type,
	corrupted,
	bad_load,
	bad_interp,
	start_msg
} str_code_t;

typedef struct b_header_s
{
	unsigned char	magic[8];
	Elf64_Xword		compressed_size;
	Elf64_Xword		decompressed_size;
	Elf64_Off		offset;				/* From the beginning of this segment */
	Elf64_Xword		key;
} b_header_t;

typedef struct mapping_range_s
{
	uintptr_t	start;
	uintptr_t	end;
} mapping_range_t;

typedef struct elf_info_s
{
	Elf64_Ehdr	*file_header;	/* File representation of a valid elf, starting at its header */
	Elf64_Ehdr	*mem_header;	/* Address of the beginning of elf header inside a loaded segment (if any) */
	uint64_t	file_data_sz;	/* Full size of elf file representation */
	Elf64_Phdr	*file_phdr;		/* Address of the beginning of phdr inside elf file representation */
	Elf64_Phdr	*mem_phdr;		/* Address of the beginning of phdr inside a loaded segment (if any) */
	void		*aslr_addr;		/* Random address for ET_DYN loading. For ET_EXEC this is NULL */
	uint64_t	load_bias;		/* Offset added to ELF virtual addresses to obtain runtime addresses */
} elf_info_t;

static const char	strings[][STRINGS_LEN] = {
	[start_msg] = "+------------------------------------------------+\n"
    "|  __        __   ___     ___    ____   __   __  |\n"
    "|  \\ \\      / /  / _ \\   / _ \\  |  _ \\  \\ \\ / /  |\n"
    "|   \\ \\ /\\ / /  | | | | | | | | | | | |  \\ V /   |\n"
    "|    \\ V  V /   | |_| | | |_| | | |_| |   | |    |\n"
    "|     \\_/\\_/     \\___/   \\___/  |____/    |_|    |\n"
    "|                                                |\n"
    "+------------------------------------------------+\n",
	[bad_stack] = "execute_packed_elf : illformed stack\n",
	[bad_auxv] = "find_auxv_field : invalid auxv\n",
	[blob_not_found] = "find_elf_blob : original elf not found or currupted\n",
	[bad_e_type] = "execute_packed_elf : unsupported ELF type\n",
	[corrupted] = "decrypt_in_place : original elf corrupted\n",
	[bad_load] = "load_elf : error during mapping original elf\n",
	[bad_interp] = "load_interpreter : invalid interpreter or error during interpreter loading\n",
};

static void	print_error(str_code_t code)
{
	sys_write(STDERR, strings[code], ft_strlen(strings[code]));
}

static Elf64_auxv_t	*find_auxv_field(Elf64_auxv_t *auxv, uint64_t type)
{
	while (auxv->a_type != 0) {
		if (auxv->a_type == type)
			return (auxv);
		auxv++;
	}

	return (NULL);
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

/* Copies a segment into the reserved writable memory and initializes its BSS. */
static void	*map_segment(Elf64_Phdr segment, const elf_info_t *elf, uint64_t page_size, int mmap_flag)
{
	uintptr_t		segment_addr;	/* Runtime address of the segment's first byte */
	uintptr_t		data_end;		/* Address right after the segment's file data, where BSS begins if present */
	uintptr_t		memory_end;		/* End of the segment, including BSS, rounded up to a page boundary */
	mapping_range_t	data_pages;		/* Runtime page range, that will be filled with file data */
	Elf64_Off		file_offset;	/* File offset corresponding to data_pages.start */
	size_t			copy_size;		/* Bytes to copy from the file, limited by the file's size */

	if (!elf || !elf->file_header)
		return (NULL);

	segment_addr = elf->load_bias + segment.p_vaddr;
	data_pages.start = round_to_page_down(segment_addr, page_size);
	memory_end = round_to_page_up(segment_addr + segment.p_memsz, page_size);

	if (segment.p_memsz == 0)
		return ((void*)data_pages.start);

	if ((intptr_t)sys_mmap(
			(void*)data_pages.start,
			memory_end - data_pages.start,
			PROT_READ | PROT_WRITE,
			mmap_flag | MAP_PRIVATE | MAP_ANONYMOUS,
			-1,
			0
		) < 0)
		return (NULL);

	if (segment.p_filesz == 0) {
		memset((void*)data_pages.start, 0, memory_end - data_pages.start);
		return ((void*)data_pages.start);
	}

	data_end = segment_addr + segment.p_filesz;
	data_pages.end = round_to_page_up(data_end, page_size);
	file_offset = segment.p_offset - (segment_addr - data_pages.start);
	if (file_offset >= elf->file_data_sz)
		return (NULL);

	/* We copy all the corresponding file content. If file
	is too small, we zero the remaining bytes of the page */
	copy_size = data_pages.end - data_pages.start;
	if (copy_size > elf->file_data_sz - file_offset)
		copy_size = elf->file_data_sz - file_offset;
	memcpy((void*)data_pages.start,
		(unsigned char*)elf->file_header + file_offset, copy_size);
	memset((void*)(data_pages.start + copy_size), 0,
		data_pages.end - data_pages.start - copy_size);

	/* We continue zeroing until the end of the segment,
	 rounded up to a page boundary */
	if (segment.p_memsz > segment.p_filesz)
		memset((void*)data_end, 0, memory_end - data_end);

	return ((void*)data_pages.start);
}

/* Apply permissions after all segments have been copied.
For shared pages, the last PT_LOAD in program header order determines
the final permissions. */
static int	apply_permissions(const elf_info_t *elf, uint64_t page_size)
{
	Elf64_Phdr		*segment;
	mapping_range_t	range;
	uintptr_t		segment_addr;
	int				prot;

	if (!elf || !elf->file_header || !elf->file_phdr || page_size == 0)
		return (-1);

	for (Elf64_Half i = 0; i < elf->file_header->e_phnum; i++) {
		segment = &elf->file_phdr[i];
		if (segment->p_type != PT_LOAD || segment->p_memsz == 0)
			continue;

		segment_addr = elf->load_bias + segment->p_vaddr;
		range.start = round_to_page_down(segment_addr, page_size);
		range.end = round_to_page_up(segment_addr + segment->p_memsz, page_size);

		/* ELF flags and mprotect flags use different bit values. */
		prot = 0;
		if (segment->p_flags & PF_R)
			prot |= PROT_READ;
		if (segment->p_flags & PF_W)
			prot |= PROT_WRITE;
		if (segment->p_flags & PF_X)
			prot |= PROT_EXEC;

		if (sys_mprotect((void*)range.start, range.end - range.start, prot) < 0)
			return (-1);
	}
	return (0);
}

/* In order to find the actual address of the ORIGINAL ELF's program header
 table once it has been mapped, we cannot rely on the presence of a PT_PHDR
 segment, since PT_PHDR is not required by the GABI.
 Therefore, we need to determine if a PT_LOAD segment contains the file
 offset of the program header table, or set it to 0. */
static uintptr_t find_phdr_addr(const elf_info_t *elf)
{
	Elf64_Phdr		*segment;
	Elf64_Off		phdr_off;
	Elf64_Xword		phdr_size;
	Elf64_Off		offset;

	phdr_off = elf->file_header->e_phoff;
	phdr_size = elf->file_header->e_phnum * sizeof(Elf64_Phdr);

	for (Elf64_Half i = 0; i < elf->file_header->e_phnum; i++) {
		segment = &elf->file_phdr[i];
		if (segment->p_type != PT_LOAD || segment->p_offset > phdr_off)
			continue;

		offset = phdr_off - segment->p_offset;
		if (offset <= segment->p_filesz && phdr_size <= segment->p_filesz - offset)
			return (elf->load_bias + segment->p_vaddr + offset);
	}
	return (0);
}

/* Copies all PT_LOAD segments from the file image into the reserved memory,
then applies their final page permissions. For semplicity, the packer must
refuse elf with a total mapping range that is too large. */
static int	map_segments(elf_info_t *elf, uint64_t page_size)
{
	int	first_pt_load = 1;

	if (!elf || !elf->file_header || !elf->file_phdr)
		return (-1);

	for (Elf64_Half i = 0; i < elf->file_header->e_phnum; i++) {
		if (elf->file_phdr[i].p_type == PT_LOAD && elf->file_phdr[i].p_memsz != 0) {
			if (first_pt_load) {
				if (map_segment(elf->file_phdr[i], elf, page_size, MAP_FIXED_NOREPLACE) == NULL)
					return (-1);
				first_pt_load = 0;
			}
			else {
				if (map_segment(elf->file_phdr[i], elf, page_size, MAP_FIXED) == NULL)
					return (-1);
			}
		}
	}

	if (apply_permissions(elf, page_size) < 0)
	    return (-1);

	elf->mem_phdr = (Elf64_Phdr*)find_phdr_addr(elf);

	return (0);
}

/* Returns a struct of mapping range covering all PT_LOAD segments,
including gaps between them and page-alignment padding.
Accordingly to the GABI, PT_LOAD entries are sorted by ascending p_vaddr,
so the first one gives the lower bound. The upper bound is the highest
p_vaddr + p_memsz among all PT_LOAD segments, rounded up to a page
boundary. */
static mapping_range_t	get_mapping_range(Elf64_Phdr *phdr, Elf64_Half phnum, uint64_t page_size)
{
	mapping_range_t	range = {0};
	uintptr_t	segment_end;
	Elf64_Half	i = 0;

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

/* Same logic than map_ET_DYN() */
static int	map_ET_EXEC(elf_info_t *elf, uint64_t page_size)
{
	mapping_range_t	range;

	if (!elf || !elf->file_header || !elf->file_phdr)
		return (-1);

	range = get_mapping_range(elf->file_phdr, elf->file_header->e_phnum, page_size);
	if ((intptr_t)sys_mmap(
			(void*)range.start,
			range.end - range.start,
			PROT_NONE,
			MAP_PRIVATE | MAP_FIXED_NOREPLACE | MAP_ANONYMOUS,
			-1,
			0
		) < 0)
		return (-1);

	elf->aslr_addr = NULL;
	elf->load_bias = 0;

	if (sys_munmap((void*)range.start, range.end - range.start) < 0)
		return (-1);

	return (map_segments(elf, page_size));
}

static Elf64_Xword	determine_global_alignement(Elf64_Phdr *phdr, Elf64_Half phnum, uint64_t page_size)
{
	Elf64_Xword	alignment;

	alignment = page_size;

	for (Elf64_Half i = 0; i < phnum; i++) {
		if (phdr[i].p_type != PT_LOAD)
			continue;
		if (phdr[i].p_align > alignment)
			alignment = phdr[i].p_align;
	}

	return (alignment);
}

static int	map_ET_DYN(elf_info_t *elf, uint64_t page_size)
{
	mapping_range_t	range;
	Elf64_Xword		alignment;
	void			*raw_addr;

	if (!elf || !elf->file_header || !elf->file_phdr)
		return (-1);

	/* ABI states that for each PT_LOAD with p_align > 1,
	p_vaddr mod p_align == p_offset mod p_align. Packer
	checks that this is true for original ELF. However,
	the load_bias for our ET_DYN must preserve this property.
	Thus we need a load_bias for which this is true :
	(load_biad + p_vaddr) mod p_align == p_offset mod p_align
	for each PT_LOAD */
	alignment = determine_global_alignement(
					elf->file_phdr,
					elf->file_header->e_phnum,
					page_size
				);

	/* We preallocate the required size at a random address,
	to ensure we have enought space to load all the PT_LOADS.
	Note that an ORIGINAL elf with no PT_LOADs must be
	refused by the packer */
	range = get_mapping_range(elf->file_phdr, elf->file_header->e_phnum, page_size);

	/* mmap returns an address aligned to page_size. To satisfy p_align as well,
	we reserve an extra "alignment" bytes. This lets us round up to the next
	multiple of "alignment" while keeping enough room for all segments
	(the rounding adjustment never exceeds "alignment"). */
	raw_addr = sys_mmap(
					NULL,
					range.end - range.start + alignment,
					PROT_NONE,
					MAP_PRIVATE | MAP_ANONYMOUS,
					-1,
					0
				);
	if ((intptr_t)raw_addr < 0)
		return (-1);

	/* Now that we have the raw address (aligned to page_size), we can round up
	to the next multiple of "alignment". However, range.start (the starting
	virtual address of the region occupied by the segments, rounded down to
	a page boundary) will then be added to this result to obtain the base
	address of our ET_DYN. We must therefore subtract range.start before
	rounding up to ensure that there is enough room for all segments. */
	elf->load_bias = round_to_page_up((uintptr_t)raw_addr - range.start, alignment);
	elf->aslr_addr = (void*)(elf->load_bias + range.start);

	/* We can free the checking reservation */
	if (sys_munmap(raw_addr, range.end - range.start + alignment) < 0)
		return (-1);

	return (map_segments(elf, page_size));
}

/* No encryption for now */
static Elf64_Ehdr	*decrypt_blob(b_header_t *b_header)
{
	return ((Elf64_Ehdr*)((unsigned char*)b_header + b_header->offset));
}

/* The stub assumes that the original ELF is encrypted/compressed
 in the last PT_LOAD. At its first offset, there must be a
 blob header containing its metadata. */
static b_header_t	*find_elf_blob_header(const elf_info_t *this)
{
	b_header_t *blob;
	Elf64_Half	phnum;

	if (!this || !this->mem_phdr)
		return (NULL);

	phnum = this->mem_header->e_phnum;
	while (phnum > 0 && this->mem_phdr[phnum - 1].p_type != PT_LOAD)
		phnum--;
	if (phnum == 0)
		return (NULL);
	blob = (b_header_t*)((unsigned char*)this->mem_phdr[phnum - 1].p_vaddr + this->load_bias);

	if (blob->magic[0] != 'W' ||
		blob->magic[1] != 'O' ||
		blob->magic[2] != 'O' ||
		blob->magic[3] != 'D' ||
		blob->magic[4] != 'Y' ||
		blob->magic[5] != '\0')
		return (NULL);

	return (blob);
}

static int	find_interpreter(char *interp_name, elf_info_t *elf)
{
	char		*interp;
	Elf64_Xword	interp_sz;

	interp_name[0] = '\0';

	for (Elf64_Half i = 0; i < elf->file_header->e_phnum; i++) {
		if (elf->file_phdr[i].p_type == PT_INTERP) {
			interp = (char*)elf->file_header + elf->file_phdr[i].p_offset;
			interp_sz = elf->file_phdr[i].p_filesz;

			if (interp_sz < 2 || interp_sz > PATH_MAX ||
				interp[interp_sz - 1] != '\0')
				return (-1);
			memcpy(interp_name, interp, interp_sz);
			break;
		}
	}
	return (0);
}

/* Accordingly to the Linux kernel behaviour, the stub ignores any
PT_INTERP in the interpreter binary */
static int	load_interpreter(elf_info_t *interp, char *interp_name, uint64_t page_size)
{
	int			interp_fd;
	struct stat	st;

	interp_fd = sys_open(interp_name, O_RDONLY);
	if (interp_fd < 0)
		return (-1);

	if (sys_fstat(interp_fd, &st) < 0)
		goto close_fd;

	interp->file_data_sz = st.st_size;
	interp->file_header = (Elf64_Ehdr*) sys_mmap(NULL,
										interp->file_data_sz,
										PROT_READ,
										MAP_PRIVATE,
										interp_fd,
										0);
	if ((intptr_t)interp->file_header < 0)
		goto close_fd;

	interp->file_phdr = (Elf64_Phdr*)((unsigned char*)interp->file_header +
		interp->file_header->e_phoff
	);

	/* For now, due to the complexity of preventing corruption of the stub
	pages caused by a mapping collision with an ET_EXEC interpreter,
	we reject this type of interpreters */
	if (interp->file_header->e_type == ET_DYN) {
		if (map_ET_DYN(interp, page_size) < 0)
			goto free_interp_load_zone;
	}
	else
		goto free_interp_load_zone;

	sys_close(interp_fd);
	return (0);

free_interp_load_zone:
	sys_munmap(interp->file_header, interp->file_data_sz);
close_fd:
	sys_close(interp_fd);
	return (-1);
}

/* decrypted_elf represents the pointer to the ORIGINAL elf file's filesystem
representation in memory.
elf_load_addr represents the pointer to the ORIGINAL elf's final load address
interp_load_addr represents the pointer to the interpreter's load address */
static int	fix_auxv(Elf64_auxv_t *auxv, const elf_info_t *elf, const elf_info_t *interp)
{
	Elf64_auxv_t	*field;

	if (!auxv || !elf || !elf->file_header || !interp)
		return (-1);

	/* 0 if no phdr table is loaded (for ET_EXECs) */
	field = find_auxv_field(auxv, AT_PHDR);
	if (field == NULL)
		return (-1);
	field->a_un.a_val = (uint64_t)elf->mem_phdr;

	field = find_auxv_field(auxv, AT_PHNUM);
	if (field == NULL)
		return (-1);
	field->a_un.a_val = (uint64_t)elf->file_header->e_phnum;

	field = find_auxv_field(auxv, AT_ENTRY);
	if (field == NULL)
		return (-1);
	field->a_un.a_val = (uint64_t)(elf->file_header->e_entry + elf->load_bias);

	/* 0 if no interpreter */
	field = find_auxv_field(auxv, AT_BASE);
	if (field == NULL)
		return (-1);
	field->a_un.a_val = (uint64_t)(interp->load_bias);

	return (0);
}

static Elf64_auxv_t	*find_auxv_start(uintptr_t *stack)
{
	/* Skip argc */
	stack++;

	/* Skip argvs */
	while (*stack != 0)
		stack++;
	stack++;

	/* Skip envps */
	while (*stack != 0)
		stack++;
	stack++;

	return ((Elf64_auxv_t*)stack);
}

static str_code_t	init_this_elf_struct(elf_info_t *this, Elf64_auxv_t *auxv)
{
	Elf64_auxv_t *auxv_field;

	auxv_field = find_auxv_field(auxv, AT_PHDR);
	if (auxv_field == NULL)
		return (bad_auxv);
	this->mem_phdr = (Elf64_Phdr*)auxv_field->a_un.a_val;

	/* The stub assumes that the packer stores the ELF Header at
	the very beginning of the binary, followed by the ELF Phdr,
	and that they will be loaded in this order in the same PT_LOAD */
	this->mem_header = (Elf64_Ehdr*)((unsigned char*)this->mem_phdr - sizeof(Elf64_Ehdr));

	/* The stub assumes thats its e_type is the same of the original elf */
	if (this->mem_header->e_type == ET_EXEC)
		this->load_bias = 0;
	else if (this->mem_header->e_type == ET_DYN)
		this->load_bias = (uint64_t)this->mem_header;
	else
		return (bad_e_type);

	return (ok);
}

_Noreturn void	execute_packed_elf(uintptr_t *stack)
{
	elf_info_t		this = {0};						/* refers to THIS running elf, the binary resulting of a packing, which entry is this stub. */
	elf_info_t		elf = {0};						/* refers to the ORIGINAL elf encrypted inside THIS binary in a "blob" format. */
	elf_info_t		interp = {0};					/* refers to the interpreter of the ORIGINAL elf */
	Elf64_auxv_t	*auxv;							/* Will contain the begin of the auxv table */
	b_header_t		*blob_header;					/* Will contain the address of the ORIGINAL elf blob header */
	char			interp_name[PATH_MAX] = {0};	/* Will contain the string of the ORIGINAL elf interpreter's name */
	void			*entry = NULL;					/* Will contain the entry address of the ORIGINAL elf or its interpreter's */
	uint64_t		page_size;						/* Will contain the page size for this execution */
	int				retval;
	Elf64_auxv_t	*auxv_field;

	sys_write(STDOUT, strings[start_msg], sizeof(strings[start_msg]));

	if (!stack) {
		print_error(bad_stack);
		sys_exit(EXIT_FAILURE);
	}

	auxv = find_auxv_start(stack);

	auxv_field = find_auxv_field(auxv, AT_PAGESZ);
	if (auxv_field == NULL) {
		print_error(bad_auxv);
		sys_exit(EXIT_FAILURE);
	}
	page_size = auxv_field->a_un.a_val;

	retval = init_this_elf_struct(&this, auxv);
	if (retval != ok) {
		print_error(retval);
		sys_exit(EXIT_FAILURE);
	}

	blob_header = find_elf_blob_header(&this);
	if (blob_header == NULL) {
		print_error(blob_not_found);
		sys_exit(EXIT_FAILURE);
	}

	elf.file_header = decrypt_blob(blob_header); // a faire
	if (elf.file_header == NULL) {
		print_error(corrupted);
		sys_exit(EXIT_FAILURE);
	}
	elf.file_data_sz = blob_header->decompressed_size;
	elf.file_phdr = (Elf64_Phdr *)((unsigned char*)elf.file_header +
					elf.file_header->e_phoff);

	if (elf.file_header->e_type == ET_EXEC)
		retval = map_ET_EXEC(&elf, page_size);
	else if (elf.file_header->e_type == ET_DYN)
		retval = map_ET_DYN(&elf, page_size);
	else {
		print_error(bad_e_type);
		sys_exit(EXIT_FAILURE);
	}

	if (retval < 0) {
		print_error(bad_load);
		sys_exit(EXIT_FAILURE);
	}

	if (find_interpreter(interp_name, &elf) < 0) {
		print_error(bad_interp);
		sys_exit(EXIT_FAILURE);
	}

	if (interp_name[0] != '\0') {
		if (load_interpreter(&interp, interp_name, page_size) < 0) {
			print_error(bad_interp);
			sys_exit(EXIT_FAILURE);
		}
		entry = (void*)(interp.file_header->e_entry + interp.load_bias);
	}
	else
		entry = (void*)(elf.file_header->e_entry + elf.load_bias);

	/* We need to update auxv data to make it match the original elf configuration.
	It will be used by the interpreter to set the elf up */
	if (fix_auxv(auxv, &elf, &interp) < 0) {
		print_error(bad_auxv);
		sys_exit(EXIT_FAILURE);
	}

	sys_munmap(interp.file_header, interp.file_data_sz);
	// sys_munmap(blob_header, blob_header->decompressed_size); Si on veut free il faut calculer la base de la page et sa fin

	/* We jump to the loaded interepreter or directly to the decrypted elf entry */
	jump_to_with_stack(entry, stack);
}
