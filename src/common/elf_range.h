#ifndef ELF_RANGE_H
# define ELF_RANGE_H

# include <elf.h>
# include <stddef.h>
# include <stdint.h>

typedef struct mapping_range_s
{
	uintptr_t	start;
	uintptr_t	end;
} mapping_range_t;

/* Requires accessible headers with PT_LOAD entries in ascending p_vaddr order,
nonzero page_size, and segment bounds that can be rounded without overflow. */
mapping_range_t	get_mapping_range(const Elf64_Phdr *phdr, Elf64_Half phnum, size_t page_size);

#endif
