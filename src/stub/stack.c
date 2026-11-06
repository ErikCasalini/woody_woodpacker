#include "exec_env.h"
#include "../ft/libft.h"

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

stub_error_t	fix_auxv(Elf64_auxv_t *auxv, const elf_info_t *elf, const elf_info_t *interp)
{
	Elf64_auxv_t	*field;

	if (!auxv || !elf || !elf->file_header || !interp)
		return (unexpected);

	field = find_auxv_field(auxv, AT_PHDR);
	if (field == NULL)
		return (bad_auxv);
	field->a_un.a_val = (uint64_t)elf->mem_phdr;

	field = find_auxv_field(auxv, AT_PHNUM);
	if (field == NULL)
		return (bad_auxv);
	field->a_un.a_val = (uint64_t)elf->file_header->e_phnum;

	field = find_auxv_field(auxv, AT_ENTRY);
	if (field == NULL)
		return (bad_auxv);
	field->a_un.a_val = (uint64_t)(elf->file_header->e_entry + elf->load_bias);

	/* 0 if no interpreter */
	field = find_auxv_field(auxv, AT_BASE);
	if (field == NULL)
		return (bad_auxv);
	field->a_un.a_val = (uint64_t)(interp->load_bias);

	return (ok);
}


stub_error_t	update_stack_args(uintptr_t *stack, Elf64_auxv_t *auxv)
{
	size_t		size;
	uint64_t	argc;
	int			i;

	argc = *stack;

	/* 64 is an aribitrary security check to avoid
	infinite loops */
	for (i = 0; i < 64; i++) {
		if (auxv->a_type == AT_NULL)
			break;
		auxv++;
	}

	if (i == 64)
		return (bad_stack);

	size = (size_t)((unsigned char*)(auxv + 1) - (unsigned char*)(stack + 3));
	ft_memmove(stack + 2, stack + 3, size);
	*(uint64_t*)stack = argc - 1;
	return (ok);
}
