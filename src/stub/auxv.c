#include "auxv.h"
#include <stddef.h>

Elf64_auxv_t	*find_auxv_start(uintptr_t *stack)
{
	/* Skip argc */
	stack++;

	/* Skip argvs */
	while (*stack != 0)
		stack++;
	stack++;

	/* Skip envps */
	while (*stack != 0)
		stack++;
	stack++;

	return ((Elf64_auxv_t*)stack);
}

Elf64_auxv_t	*find_auxv_field(Elf64_auxv_t *auxv, uint64_t type)
{
	while (auxv->a_type != 0) {
		if (auxv->a_type == type)
			return (auxv);
		auxv++;
	}

	return (NULL);
}

int	fix_auxv(Elf64_auxv_t *auxv, const elf_info_t *elf, const elf_info_t *interp)
{
	Elf64_auxv_t	*field;

	if (!auxv || !elf || !elf->file_header || !interp)
		return (-1);

	field = find_auxv_field(auxv, AT_PHDR);
	if (field == NULL)
		return (-1);
	field->a_un.a_val = (uint64_t)elf->mem_phdr;

	field = find_auxv_field(auxv, AT_PHNUM);
	if (field == NULL)
		return (-1);
	field->a_un.a_val = (uint64_t)elf->file_header->e_phnum;

	field = find_auxv_field(auxv, AT_ENTRY);
	if (field == NULL)
		return (-1);
	field->a_un.a_val = (uint64_t)(elf->file_header->e_entry + elf->load_bias);

	/* 0 if no interpreter */
	field = find_auxv_field(auxv, AT_BASE);
	if (field == NULL)
		return (-1);
	field->a_un.a_val = (uint64_t)(interp->load_bias);

	return (0);
}
