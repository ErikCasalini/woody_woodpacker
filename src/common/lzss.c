#include "lzss.h"
#include "../ft/libft.h"

size_t	lzss_decompress(const uint8_t *src, size_t src_len,
			uint8_t *dst, size_t dst_cap)
{
	size_t		in;
	size_t		out;
	uint8_t		flags;
	int		bit;

	if (src_len == 0)
		return (0);
	if (src == NULL || dst == NULL)
		return (LZSS_ERROR);
	in = 0;
	out = 0;
	while (in < src_len)
	{
		flags = src[in++];	/* one flag byte describes the next 8 tokens */
		for (bit = 0; bit < 8; bit++)
		{
			/* trailing flag bits beyond the real data are 0 (literals) */
			if (in >= src_len && !((flags >> bit) & 1))
				return (out);
			if ((flags >> bit) & 1)
			{
				uint32_t	distance;
				uint32_t	length;
				uint8_t		b0;
				uint8_t		b1;

				if (in + 2 > src_len)
					return (LZSS_ERROR);
				/* rebuild (distance, length) from the 2-byte match token */
				b0 = src[in++];
				b1 = src[in++];
				distance = (((uint32_t)(b1 >> 4) & 0x0F) << 8 | b0) + 1;
				length = ((uint32_t)(b1 & 0x0F)) + LZSS_MIN_MATCH;
				if (distance > out)	/* match must point inside what we decoded */
					return (LZSS_ERROR);
				if (length > dst_cap - out)
					return (LZSS_ERROR);
				/* copy byte-by-byte so overlapping matches work (RLE-style) */
				for (uint32_t k = 0; k < length; k++)
				{
					dst[out] = dst[out - distance];
					out++;
				}
			}
			else
			{
				if (in >= src_len)
					return (LZSS_ERROR);
				if (out >= dst_cap)
					return (LZSS_ERROR);
				dst[out++] = src[in++];
			}
		}
	}
	return (out);
}

/*
** Greedy longest-match search: scan the window [pos-WINDOW, pos) and return
** the length of the best match for the bytes at pos (0 if none worth it),
** writing its distance into *out_distance.
*/
static uint32_t	find_match(const uint8_t *src, size_t src_len,
			size_t pos, uint32_t *out_distance)
{
	size_t		win_start;
	size_t		cand;
	uint32_t	best_len;
	uint32_t	best_dist;
	size_t		max_here;

	best_len = 0;
	best_dist = 0;
	win_start = (pos > LZSS_WINDOW) ? pos - LZSS_WINDOW : 0;
	max_here = src_len - pos;
	if (max_here > LZSS_MAX_MATCH)
		max_here = LZSS_MAX_MATCH;
	if (max_here < LZSS_MIN_MATCH)
		return (0);
	cand = win_start;
	while (cand < pos)
	{
		uint32_t	len;

		len = 0;
		while (len < max_here && src[cand + len] == src[pos + len])
			len++;
		if (len > best_len)
		{
			best_len = len;
			best_dist = (uint32_t)(pos - cand);
			if (best_len == max_here)
				break;
		}
		cand++;
	}
	if (best_len < LZSS_MIN_MATCH)
		return (0);
	*out_distance = best_dist;
	return (best_len);
}

static size_t	copy_uncompressed(const uint8_t *src, size_t src_len,
			uint8_t *dst, size_t dst_cap)
{
	if (src_len > dst_cap)
		return (LZSS_ERROR);
	ft_memcpy(dst, src, src_len);
	return (LZSS_ERROR);
}

/*
** Returns compressed size on success, LZSS_ERROR otherwise.
** On failure, src is copied to dst if both pointers are valid,
** buffers do not overlap and dst_cap >= src_len.
** Otherwise, no copy is guaranteed.
*/
size_t	lzss_compress(const uint8_t *src, size_t src_len,
			uint8_t *dst, size_t dst_cap)
{
	size_t		in;
	size_t		out;
	size_t		flag_pos;
	int		bit;

	if (src_len == 0)
		return (LZSS_ERROR);
	if (src == NULL || dst == NULL)
		return (LZSS_ERROR);
	in = 0;
	out = 0;
	while (in < src_len)
	{
		if (out >= dst_cap)
			return (copy_uncompressed(src, src_len, dst, dst_cap));
		flag_pos = out;		/* reserve the flag byte, filled in as we go */
		dst[out++] = 0;
		bit = 0;
		while (bit < 8 && in < src_len)
		{
			uint32_t	distance;
			uint32_t	length;

			distance = 0;
			length = find_match(src, src_len, in, &distance);
			if (length >= LZSS_MIN_MATCH)
			{
				if (dst_cap - out < 2)
					return (copy_uncompressed(src, src_len, dst, dst_cap));
				/* mark this token as a match and pack distance+length */
				dst[flag_pos] |= (uint8_t)(1 << bit);
				dst[out++] = (uint8_t)((distance - 1) & 0xFF);
				dst[out++] = (uint8_t)((((distance - 1) >> 8) & 0x0F) << 4
						| ((length - LZSS_MIN_MATCH) & 0x0F));
				in += length;
			}
			else
			{
				if (out >= dst_cap)
					return (copy_uncompressed(src, src_len, dst, dst_cap));
				dst[out++] = src[in++];
			}
			bit++;
		}
	}
	if (out >= src_len)
		return (copy_uncompressed(src, src_len, dst, dst_cap));
	return (out);
}
