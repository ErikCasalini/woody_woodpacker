#include "chacha20.h"

/* 32-bit left rotation (n in [1, 31]). */
static uint32_t	rotl32(uint32_t x, int n)
{
	return ((x << n) | (x >> (32 - n)));
}

/* Read 4 bytes as a little-endian 32-bit word. */
static uint32_t	load32_le(const uint8_t *p)
{
	return ((uint32_t)p[0]
		| ((uint32_t)p[1] << 8)
		| ((uint32_t)p[2] << 16)
		| ((uint32_t)p[3] << 24));
}

/* Write a 32-bit word as 4 little-endian bytes. */
static void	store32_le(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v & 0xff);
	p[1] = (uint8_t)((v >> 8) & 0xff);
	p[2] = (uint8_t)((v >> 16) & 0xff);
	p[3] = (uint8_t)((v >> 24) & 0xff);
}

/* The ARX quarter-round: mixes 4 words of the state with
   add / rotate / xor only (no table, no S-box). */
static void	quarter_round(uint32_t *s, int a, int b, int c, int d)
{
	s[a] += s[b];
	s[d] ^= s[a];
	s[d] = rotl32(s[d], 16);
	s[c] += s[d];
	s[b] ^= s[c];
	s[b] = rotl32(s[b], 12);
	s[a] += s[b];
	s[d] ^= s[a];
	s[d] = rotl32(s[d], 8);
	s[c] += s[d];
	s[b] ^= s[c];
	s[b] = rotl32(s[b], 7);
}

/* The "mixer": from a 16-word starting grid, run 20 rounds of
   quarter-rounds (10 column rounds + 10 diagonal rounds interleaved),
   then feed-forward (add the starting grid back) and serialize to
   64 bytes of keystream. */
static void	chacha20_block(const uint32_t init[16], uint8_t out[64])
{
	uint32_t	s[16];
	int			i;

	for (i = 0; i < 16; i++)
		s[i] = init[i];
	for (i = 0; i < 10; i++) {
		/* column rounds */
		quarter_round(s, 0, 4, 8, 12);
		quarter_round(s, 1, 5, 9, 13);
		quarter_round(s, 2, 6, 10, 14);
		quarter_round(s, 3, 7, 11, 15);
		/* diagonal rounds */
		quarter_round(s, 0, 5, 10, 15);
		quarter_round(s, 1, 6, 11, 12);
		quarter_round(s, 2, 7, 8, 13);
		quarter_round(s, 3, 4, 9, 14);
	}
	for (i = 0; i < 16; i++)
		store32_le(out + i * 4, s[i] + init[i]);
}

void	chacha20_crypt(const uint8_t key[CHACHA20_KEY_SIZE],
			const uint8_t nonce[CHACHA20_NONCE_SIZE],
			uint32_t counter,
			uint8_t *data, size_t len)
{
	uint32_t	init[16];
	uint8_t		block[64];
	size_t		off;
	size_t		chunk;
	size_t		k;

	/* Constant "expand 32-byte k" */
	init[0] = 0x61707865;
	init[1] = 0x3320646e;
	init[2] = 0x79622d32;
	init[3] = 0x6b206574;
	for (k = 0; k < 8; ++k)
		init[4 + k] = load32_le(key + k * 4);
	init[12] = counter;
  for (size_t n = 0; n < 3; ++n)
    init[n + 13] = load32_le(nonce + n * 4);

	off = 0;
	while (off < len) {
		chacha20_block(init, block);
		chunk = len - off;
		if (chunk > 64)
			chunk = 64;
		for (k = 0; k < chunk; k++)
			data[off + k] ^= block[k];
		off += chunk;
		init[12]++;
	}
}
