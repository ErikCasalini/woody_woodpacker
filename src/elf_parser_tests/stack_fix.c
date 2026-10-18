#include "elf_types.h"
#include "errors.h"
#include "stack_fix.h"
#include <sys/auxv.h>
#include <string.h>

#include <unistd.h>

/*
AT_PHDR
AT_PHNUM
AT_BASE
AT_ENTRY
AT_EXECFN ?
*/

static int	set_auxv(t_auxv *auxv_array, t_auxv auxv)
{
	for (size_t i = 0; auxv_array[i].type != AT_NULL; i++) {
		if (auxv_array[i].type == auxv.type) {
			auxv_array[i].val = auxv.val;
			return (0);
		}
	}
	return (1);
}

static uint64_t	find_AT_PHDR(t_ELF64 *elf, uint64_t load_bias)
{
	uint64_t				e_phoff = elf->header.program_header_offset;
	t_ELF64_program_header	segment;

	for (int i = 0; i < elf->header.program_header_number; i++) {
		segment = elf->program_header[i];

		if (segment.type != PT_LOAD)
			continue;

		if (segment.offset <= e_phoff &&
			segment.filesz > (e_phoff - segment.offset))
			return (segment.vaddr + (e_phoff - segment.offset) + load_bias);
	}

	return (0);
}

int	update_stack(t_ELF64 *elf, uint8_t **stack, uint64_t load_bias, uint64_t intrp_base_address)
{
	t_auxv		*auxv_array;
	t_auxv		auxv;
	t_auxv		*auxv_array_top;
	uint8_t		**argc_ptr = stack;
	uint64_t	argc = *(uint64_t*)stack;
	size_t		size;

	if (elf == NULL || stack == NULL)
		return (1);

	while (*stack != NULL)
		stack++;
	stack++;

	while (*stack != NULL)
		stack++;
	stack++;

	auxv_array = (t_auxv *)stack;

	auxv.type = AT_PHDR;
	auxv.val = find_AT_PHDR(elf, load_bias);
	set_auxv(auxv_array, auxv);

	auxv.type = AT_PHNUM;
	auxv.val = elf->header.program_header_number;
	set_auxv(auxv_array, auxv);

	auxv.type = AT_BASE;
	auxv.val = intrp_base_address;
	set_auxv(auxv_array, auxv);

	auxv.type = AT_ENTRY;
	auxv.val = elf->header.entry + load_bias;
	set_auxv(auxv_array, auxv);

	auxv_array_top = auxv_array;
	while (auxv_array_top->type != AT_NULL)
		auxv_array_top++;

	size = (uint8_t*)(auxv_array_top + 1) - (uint8_t*)(argc_ptr + 1);
	// UTILISER LIBFT
	memmove((char**)argc_ptr, (char**)argc_ptr + 1, size);
	*(uint64_t*)argc_ptr = argc - 1;

	return (0);
}
