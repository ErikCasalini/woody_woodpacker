#include "loader.h"
#include <stddef.h>
#include <sys/mman.h>
#include "../common/elf_range.h"
#include "../common/page.h"
#include "../ft/libft.h"
#include "syscalls/syscalls.h"

/* Copies a segment into the reserved writable memory and initializes its BSS */
static void	*map_segment(Elf64_Phdr segment, const elf_info_t *elf, uint64_t page_size, int mmap_flag)
{
	uintptr_t		segment_addr;	/* Runtime address of the segment's first byte */
	uintptr_t		data_end;		/* Address right after the segment's file data, where BSS begins if present */
	uintptr_t		memory_end;		/* End of the segment, including BSS, rounded up to a page boundary */
	mapping_range_t	data_pages;		/* Runtime page range, that will be filled with file data */
	Elf64_Off		file_off;		/* File offset corresponding to data_pages.start */
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
	file_off = segment.p_offset - (segment_addr - data_pages.start);
	if (file_off >= elf->file_data_sz)
		return (NULL);

	/* We copy all the corresponding file content. If file
	is too small, we zero the remaining bytes of the page */
	copy_size = data_pages.end - data_pages.start;
	if (copy_size > elf->file_data_sz - file_off)
		copy_size = elf->file_data_sz - file_off;
	memcpy((void*)data_pages.start,
		(unsigned char*)elf->file_header + file_off, copy_size);
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
the final permissions */
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
 offset of the program header table, or set it to load_bias if not found
 (linux seems doing this). */
static uintptr_t find_phdr_addr(const elf_info_t *elf)
{
	Elf64_Phdr		*segment;
	Elf64_Off		phdr_off;
	Elf64_Xword		phdr_size;
	Elf64_Off		offset;

	phdr_off = elf->file_header->e_phoff;
	phdr_size = elf->file_header->e_phnum * elf->file_header->e_phentsize;

	for (Elf64_Half i = 0; i < elf->file_header->e_phnum; i++) {
		segment = &elf->file_phdr[i];
		if (segment->p_type != PT_LOAD || segment->p_offset > phdr_off)
			continue;

		offset = phdr_off - segment->p_offset;
		if (offset < segment->p_filesz)
			return (elf->load_bias + segment->p_vaddr + offset);
	}
	return (elf->load_bias);
}

/* Copies all PT_LOAD segments from the file image into the reserved memory,
then applies their final page permissions. */
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

/* Same logic than map_ET_DYN() */
int	map_ET_EXEC(elf_info_t *elf, uint64_t page_size)
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

	elf->base_addr = (void*)range.start;
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

static inline int reservation_size_overflows(mapping_range_t range, size_t alignment)
{
	if (alignment > SIZE_MAX - (range.end - range.start))
		return (1);
	return (0);
}

int	map_ET_DYN(elf_info_t *elf, uint64_t page_size)
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
	Note that an ORIGINAL elf with no PT_LOADs or with
	overflowing vaddr + memsz must be refused by the packer */
	range = get_mapping_range(elf->file_phdr, elf->file_header->e_phnum, page_size);

	if (reservation_size_overflows(range, alignment))
		return (-1);

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
	elf->base_addr = (void*)(elf->load_bias + range.start);

	/* We can free the checking reservation */
	if (sys_munmap(raw_addr, range.end - range.start + alignment) < 0)
		return (-1);

	return (map_segments(elf, page_size));
}
