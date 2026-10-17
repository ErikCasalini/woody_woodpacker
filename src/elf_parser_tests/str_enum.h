#ifndef PRINT_ENUM_H
# define PRINT_ENUM_H

# include "elf_types.h"

const char	*str_class(t_elf_class elf_class);
const char	*str_data(t_elf_data data);
const char	*str_version(t_elf_version version);
const char	*str_os_abi(t_elf_os_abi os_abi);
const char	*str_type(t_elf_type type);
const char	*str_machine(t_elf_machine machine);
const char	*str_program_type(t_elf_program_type type);
const char	*str_program_flags(t_elf_program_flags flags);
const char	*str_d_tag(t_elf_d_tag d_tag);
const char	*str_rel_type(t_rel_type rel_type);

#endif
