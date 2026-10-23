#ifndef PAGE_H
# define PAGE_H

# include <stddef.h>

static inline uintptr_t	round_to_page_up(uintptr_t value, size_t page_size)
{
	uintptr_t tmp;

	tmp = value % page_size;
	if (tmp == 0)
		return (value);

	return (value + page_size - tmp);
}

static inline uintptr_t	round_to_page_down(uintptr_t value, size_t page_size)
{
	return (value - (value % page_size));
}

#endif
