#ifndef PAYLOAD_H
# define PAYLOAD_H

# include <elf.h>
# include "../common/chacha20.h"

# define PAYLOAD_MAGIC "WOODY"

typedef struct payload_header_s
{
	unsigned char	magic[8];
	int				is_compressed;
	Elf64_Xword		packed_size;
	Elf64_Xword		original_size;
	Elf64_Off		offset;				/* From the beginning of this segment */
} payload_header_t;

typedef struct packed_payload_s
{
	payload_header_t	header;
	uint8_t				*data;
	uint8_t				key[CHACHA20_KEY_SIZE];
	uint8_t				nonce[CHACHA20_NONCE_SIZE]; /* Will be always considered an empty array */

} packed_payload_t;

#endif
