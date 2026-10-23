#ifndef STUB_AUXV_H
# define STUB_AUXV_H

# include "elf_info.h"

Elf64_auxv_t	*find_auxv_start(uintptr_t *stack);
Elf64_auxv_t	*find_auxv_field(Elf64_auxv_t *auxv, uint64_t type);
int				fix_auxv(Elf64_auxv_t *auxv, const elf_info_t *elf, const elf_info_t *interp);

#endif
