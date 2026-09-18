#ifndef HEAP_ARENA_H
# define HEAP_ARENA_H

# include <stddef.h>
# include "elf_types.h"

typedef struct s_arena {
	char		*data;
	size_t		used;
	size_t		capacity;

} t_arena;

typedef struct s_context {
	t_arena					*arena;

	t_ELF64_program_header	*phdr;
	size_t					phdr_count;

	size_t					segments_count;

} t_context;

void *alloc_arena(t_arena *arena, size_t size, size_t alignement);

#endif
