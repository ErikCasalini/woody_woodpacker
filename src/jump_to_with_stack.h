#ifndef JUMP_TO_INTERP_H
# define JUMP_TO_INTERP_H

# include "custom_types.h"

extern _Noreturn void jump_to_with_stack(void *entry, void *stack_addr);

#endif
