#ifndef PAYLOAD_H
# define PAYLOAD_H

# include <elf.h>

# define PAYLOAD_MAGIC "WOODY"

typedef struct payload_header_s
{
	unsigned char	magic[8];
	Elf64_Xword		compressed_size;
	Elf64_Xword		decompressed_size;
	Elf64_Off		offset;				/* From the beginning of this segment */
	Elf64_Xword		key;
} payload_header_t;

#endif
