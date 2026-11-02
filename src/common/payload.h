#ifndef PAYLOAD_H
# define PAYLOAD_H

# include <elf.h>
# include <stdint.h>

# define PAYLOAD_MAGIC "WOODY"

typedef struct payload_header_s
{
	unsigned char	magic[8];
	Elf64_Xword		compressed_size;
	Elf64_Xword		decompressed_size;
	Elf64_Off		offset;				/* From the beginning of this segment */
	uint8_t			key[32];
} payload_header_t;

#endif
