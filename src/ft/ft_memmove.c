#include <stddef.h>
#include "libft.h"

/* For Clang to use */
void	*memmove(void *dest, const void *src, size_t n)
{
	if (dest > src)
	{
		while (n)
		{
			((unsigned char *)dest)[n - 1] = ((unsigned char *)src)[n - 1];
			n--;
		}
		return (dest);
	}
	return (memcpy(dest, src, n));
}

void	*ft_memmove(void *dest, const void *src, size_t n)
{
	if (dest > src)
	{
		while (n)
		{
			((unsigned char *)dest)[n - 1] = ((unsigned char *)src)[n - 1];
			n--;
		}
		return (dest);
	}
	return (ft_memcpy(dest, src, n));
}
