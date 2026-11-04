#ifndef STUB_INTERPRETER_H
# define STUB_INTERPRETER_H

# include "elf_info.h"
# include "messages.h"

# define PATH_MAX 4096

stub_error_t	find_interpreter(char *interp_name, const elf_info_t *elf);
stub_error_t	load_interpreter(elf_info_t *interp, const char *interp_name, size_t page_size);

#endif
