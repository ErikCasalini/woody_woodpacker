#ifndef STUB_LOADER_H
# define STUB_LOADER_H

# include "elf_info.h"

int	map_ET_EXEC(elf_info_t *elf, size_t page_size);
int	map_ET_DYN(elf_info_t *elf, size_t page_size);

#endif
