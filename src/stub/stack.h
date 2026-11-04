#ifndef STUB_STACK_H
# define STUB_STACK_H

# include "elf_info.h"
# include "messages.h"

Elf64_auxv_t	*find_auxv_start(uintptr_t *stack);
Elf64_auxv_t	*find_auxv_field(Elf64_auxv_t *auxv, uint64_t type);
stub_error_t	fix_auxv(Elf64_auxv_t *auxv, const elf_info_t *elf, const elf_info_t *interp);
stub_error_t	update_stack_args(uintptr_t *stack, Elf64_auxv_t *auxv);

#endif
