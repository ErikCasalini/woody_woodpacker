#ifndef STUB_INFO_H
# define STUB_INFO_H

# include <elf.h>
# include <stdint.h>

typedef struct stub_info_s
{
	Elf64_Ehdr	*header;	/* Represents the runtime address of the ELF Header of the packed ELF */
	Elf64_Phdr	*phdr;		/* Represents the runtime address of the ELF PHDR of the packed ELF */
	uintptr_t	load_bias;	/* Added to the packed ELF's p_vaddr to locate its runtime contents. 0 for ET_EXEC */
} stub_info_t;

#endif
