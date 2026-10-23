#include "elf_range.h"
#include "page.h"

/* Returns a struct of mapping range covering all PT_LOAD segments,
including gaps between them and page-alignment padding.
Accordingly to the GABI, PT_LOAD entries are sorted by ascending p_vaddr,
so the first one gives the lower bound. The upper bound is the highest
p_vaddr + p_memsz among all PT_LOAD segments, rounded up to a page
boundary. */
mapping_range_t	get_mapping_range(const Elf64_Phdr *phdr, Elf64_Half phnum, uint64_t page_size)
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
