#ifndef STUB_INTERPRETER_H
# define STUB_INTERPRETER_H

# include "elf_info.h"

# define PATH_MAX 4096

int	find_interpreter(char *interp_name, elf_info_t *elf);
int	load_interpreter(elf_info_t *interp, char *interp_name, uint64_t page_size);

#endif
