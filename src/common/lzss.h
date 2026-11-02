#ifndef LZSS_H
# define LZSS_H

# include <stddef.h>
# include <stdint.h>

# define LZSS_WINDOW     4096                    /* max back-reference distance */
# define LZSS_MIN_MATCH  3                       /* shortest match worth encoding */
# define LZSS_MAX_MATCH  (LZSS_MIN_MATCH + 15)   /* 4-bit length field -> +0..15 */

# define LZSS_ERROR      ((size_t)-1)            /* returned on any failure */

size_t	lzss_compress(const uint8_t *src, size_t src_len,
			uint8_t *dst, size_t dst_cap);

size_t	lzss_decompress(const uint8_t *src, size_t src_len,
			uint8_t *dst, size_t dst_cap);

#endif
