#ifndef STUB_ELF_INFO_H
# define STUB_ELF_INFO_H

# include <elf.h>
# include <stddef.h>
# include <stdint.h>

typedef struct elf_info_s
{
	Elf64_Ehdr	*file_header;	/* File representation of a valid elf, starting at its header */
	size_t		file_sz;		/* Full size of elf file representation */
	Elf64_Phdr	*file_phdr;		/* Address of the beginning of phdr inside elf file representation */
	Elf64_Phdr	*mem_phdr;		/* Address of the beginning of phdr inside a loaded segment (if any) */
	uintptr_t	load_bias;		/* Offset added to ELF virtual addresses to obtain runtime addresses */
} elf_info_t;

#endif
