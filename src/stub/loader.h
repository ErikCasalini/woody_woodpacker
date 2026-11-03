#ifndef STUB_LOADER_H
# define STUB_LOADER_H

# include "elf_info.h"
# include "messages.h"

stub_error_t	map_ET_EXEC(elf_info_t *elf, size_t page_size);
stub_error_t	map_ET_DYN(elf_info_t *elf, size_t page_size);

#endif
