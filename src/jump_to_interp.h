#ifndef JUMP_TO_INTERP_H
# define JUMP_TO_INTERP_H

# include <stdint.h>

extern void jump_to_with_stack(uint64_t entry, uint64_t stack_addr)
			__attribute__((noreturn));

#endif
