#include "stdlib.h"
#include "heap_arena.h"

static size_t	round_up(size_t offset, size_t alignment)
{
	return ((offset + alignment - 1) & ~(alignment - 1));
}

void *alloc_arena(t_arena *arena, size_t size, size_t alignement)
{
	size_t offset;

	offset = round_up(arena->used, alignement);
	if (offset > arena->capacity ||
		size > arena->capacity - offset)
		return (NULL);

	arena->used = offset + size;
	return (arena->data + offset);
}
