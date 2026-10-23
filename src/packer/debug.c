#include "debug.h"

#ifdef DEBUG
#include <inttypes.h>
#include <stdio.h>

static void	print_output_segment(const char *name, const Elf64_Phdr *segment)
{
	printf("\n%s\n", name);
	printf("  p_type:   %" PRIu32 "\n", segment->p_type);
	printf("  p_flags:  0x%08" PRIx32 " (%c%c%c)\n", segment->p_flags,
		(segment->p_flags & PF_R) ? 'R' : '-',
		(segment->p_flags & PF_W) ? 'W' : '-',
		(segment->p_flags & PF_X) ? 'X' : '-');
	printf("  p_offset: 0x%016" PRIx64 "\n", segment->p_offset);
	printf("  p_vaddr:  0x%016" PRIx64 "\n", segment->p_vaddr);
	printf("  p_paddr:  0x%016" PRIx64 "\n", segment->p_paddr);
	printf("  p_filesz: 0x%016" PRIx64 "\n", segment->p_filesz);
	printf("  p_memsz:  0x%016" PRIx64 "\n", segment->p_memsz);
	printf("  p_align:  0x%016" PRIx64 "\n", segment->p_align);
}

void	print_output_layout(const elf_layout_t *output)
{
	const Elf64_Ehdr	*header = &output->header;

	printf("ELF header\n");
	printf("  e_ident:    ");
	for (size_t i = 0; i < EI_NIDENT; i++)
		printf(" %02x", (unsigned int)header->e_ident[i]);
	printf("\n");
	printf("  e_type:      %u\n", (unsigned int)header->e_type);
	printf("  e_machine:   %u\n", (unsigned int)header->e_machine);
	printf("  e_version:   %" PRIu32 "\n", header->e_version);
	printf("  e_entry:     0x%016" PRIx64 "\n", header->e_entry);
	printf("  e_phoff:     0x%016" PRIx64 "\n", header->e_phoff);
	printf("  e_shoff:     0x%016" PRIx64 "\n", header->e_shoff);
	printf("  e_flags:     0x%08" PRIx32 "\n", header->e_flags);
	printf("  e_ehsize:    0x%04x\n", (unsigned int)header->e_ehsize);
	printf("  e_phentsize: 0x%04x\n", (unsigned int)header->e_phentsize);
	printf("  e_phnum:     %u\n", (unsigned int)header->e_phnum);
	printf("  e_shentsize: 0x%04x\n", (unsigned int)header->e_shentsize);
	printf("  e_shnum:     %u\n", (unsigned int)header->e_shnum);
	printf("  e_shstrndx:  %u\n", (unsigned int)header->e_shstrndx);
	print_output_segment("phdrs_segment (PT_LOAD)", &output->phdrs_segment);
	print_output_segment("stub_segment (PT_LOAD)", &output->stub_segment);
	print_output_segment("payload_segment (PT_LOAD)", &output->payload_segment);
	print_output_segment("gnu_stack_segment (GNU_STACK)", &output->gnu_stack_segment);
}
#endif
