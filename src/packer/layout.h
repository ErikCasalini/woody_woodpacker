#ifndef PACKER_LAYOUT_H
# define PACKER_LAYOUT_H

# include <elf.h>
# include <stdint.h>
# include "file.h"

typedef struct elf_layout_s
{
	Elf64_Ehdr	header;
	Elf64_Phdr	phdrs_segment;
	Elf64_Phdr	stub_segment;
	Elf64_Phdr	payload_segment;
	Elf64_Phdr	gnu_stack_segment;
} elf_layout_t;

error_t	init_output_headers(elf_layout_t *output, const mapped_file_t *target, const Elf64_Shdr *packer_stub_section, uint64_t page_size);

#endif
