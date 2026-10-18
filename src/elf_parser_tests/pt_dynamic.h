#ifndef PT_DYNAMIC_H
# define PT_DYNAMIC_H

# include <stdint.h>

#define ELF64_R_SYM(i)     ((i)>>32)
#define ELF64_R_TYPE(i)    ((i)&0xffffffffL)
#define ELF64_R_INFO(s,t)  (((s)<<32)+((t)&0xffffffffL))

typedef enum e_reloc_table_status {
	not_present = 0,
	valid = 1,
	invalid = 2
} t_reloc_table_status;

typedef struct s_reloc_table_info {
	t_reloc_table_status	status;
	uint64_t				addr;
	uint64_t				size;
	uint64_t				ent_size;
} t_reloc_table_info;

t_error print_relocations_fields(int file, t_ELF64 elf, uint64_t base_addr);;

#endif
