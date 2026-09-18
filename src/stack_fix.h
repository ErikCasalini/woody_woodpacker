#ifndef STACK_FIX_H
# define STACK_FIX_H

# include <stdint.h>

typedef struct	s_auxv {
    uint64_t	type;
    uint64_t	val;
} 				t_auxv;

int	update_stack(t_ELF64 *elf, uint8_t **stack, uint64_t load_bias, uint64_t base_address);

#endif
