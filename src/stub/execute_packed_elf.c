#include <stddef.h>
#include "../common/elf_range.h"
#include "../common/page.h"
#include "exec_env.h"
#include "interpreter.h"
#include "key.h"
#include "jump_to_with_stack.h"
#include "loader.h"
#include "messages.h"
#include "payload.h"
#include "stub_info.h"
#include "syscalls/syscalls.h"
#include "../common/chacha20.h"

#define EXIT_FAILURE 1

static _Noreturn void	fail(stub_error_t error)
{
	print_error(error);
	sys_exit(EXIT_FAILURE);
}

static stub_error_t	init_stub_info(stub_info_t *stub, Elf64_auxv_t *auxv)
{
	Elf64_auxv_t *auxv_field;

	auxv_field = find_auxv_field(auxv, AT_PHDR);
	if (auxv_field == NULL)
		return (bad_auxv);
	stub->phdr = (Elf64_Phdr*)auxv_field->a_un.a_val;

	/* The stub assumes that the packer stores the ELF Header at
	the very beginning of the binary, followed by the ELF Phdr,
	and that they will be loaded in this order in the same PT_LOAD */
	stub->header = (Elf64_Ehdr*)((unsigned char*)stub->phdr - sizeof(Elf64_Ehdr));

	/* The stub assumes thats its e_type is the same of the original elf */
	if (stub->header->e_type == ET_EXEC)
		stub->load_bias = 0;
	else if (stub->header->e_type == ET_DYN)
		stub->load_bias = (uintptr_t)stub->header;
	else
		return (bad_e_type);

	return (ok);
}

static stub_error_t	munmap_files(elf_info_t *interp, elf_info_t *elf, packed_payload_t *payload, size_t page_size)
{
	int				error = 0;
	mapping_range_t	range;

	if (interp && interp->file_header) {
		range.start = round_to_page_down((uintptr_t)interp->file_header, page_size);
		range.end = round_to_page_up((uintptr_t)interp->file_header + interp->file_sz, page_size);
		if (sys_munmap((void*)range.start, range.end - range.start) < 0)
			error = -1;
	}

	if (elf && elf->file_header) {
		range.start = round_to_page_down((uintptr_t)elf->file_header, page_size);
		range.end = round_to_page_up((uintptr_t)elf->file_header + elf->file_sz, page_size);
		if (sys_munmap((void*)range.start, range.end - range.start) < 0)
			error = -1;
	}

	if (payload && payload->data) {
		range.start = round_to_page_down((uintptr_t)payload->data - payload->header.offset, page_size);
		range.end = round_to_page_up((uintptr_t)payload->data + payload->header.compressed_size, page_size);
		if (sys_munmap((void*)range.start, range.end - range.start) < 0)
			error = -1;
	}

	if (error)
		return (bad_munmap);

	return (ok);
}

_Noreturn void	execute_packed_elf(uintptr_t *stack)
{
	stub_info_t			stub = {0};						/* Refers THIS binary (packed elf loaded by the kernel)  */
	packed_payload_t	payload = {0};					/* Refers to the packed ORGINAL elf loaded inside THIS binary as a payload*/
	elf_info_t			elf = {0};						/* Refers to the unpacked ORIGINAL elf*/
	elf_info_t			interp = {0};					/* Refers to the interpreter of the ORIGINAL elf */
	Elf64_auxv_t		*auxv;							/* Will contain the begin of the auxv table */
	char				interp_name[PATH_MAX] = {0};	/* Will contain the string of the ORIGINAL elf interpreter's name */
	void				*entry = NULL;					/* Will contain the entry address of the ORIGINAL elf or its interpreter's */
	size_t				page_size;						/* Will contain the page size for this execution */
	stub_error_t		error;
	Elf64_auxv_t		*auxv_field;

	print_start_msg();

	if (!stack)
		fail(bad_stack);
	/* argc */
	if (stack[0] < 2)
	    fail(bad_key);

	auxv = find_auxv_start(stack);

	auxv_field = find_auxv_field(auxv, AT_PAGESZ);
	if (auxv_field == NULL)
		fail(bad_auxv);
	page_size = auxv_field->a_un.a_val;

	error = init_stub_info(&stub, auxv);
	if (error != ok)
		fail(error);

	error = extract_payload(&payload, &stub);
	if (error != ok)
		fail(error);

	/* We assume argv[1] is the key */
	error = convert_arg_to_key((char*)stack[2], payload.key);
	if (error != ok)
		fail(error);

	error = unpack_payload(&payload, &elf);
	if (error != ok)
		fail(error);

	if (elf.file_header->e_type == ET_EXEC)
		error = map_ET_EXEC(&elf, page_size);
	else if (elf.file_header->e_type == ET_DYN)
		error = map_ET_DYN(&elf, page_size);
	else
		fail(bad_e_type);

	if (error != ok)
		fail(error);

	error = find_interpreter(interp_name, &elf);
	if (error != ok)
		fail(error);

	/* This stub assumes that interpreter is a valid ELF */
	if (interp_name[0] != '\0') {
		error = load_interpreter(&interp, interp_name, page_size);
		if (error != ok)
			fail(error);
		entry = (void*)(interp.file_header->e_entry + interp.load_bias);
	}
	else
		/* ORIGINAL ELF e_entry must be checked by the packer */
		entry = (void*)(elf.file_header->e_entry + elf.load_bias);

	/* We need to update auxv data to make it match the original elf configuration.
	It will be used by the interpreter to set the elf up */
	error = fix_auxv(auxv, &elf, &interp);
	if (error != ok)
		fail(error);

	error = update_stack_args(stack, auxv);
	if (error != ok)
		fail(error);

	error = munmap_files(&interp, &elf, &payload, page_size);
	if (error != ok)
		fail(error);

	/* We jump to the loaded interepreter or directly to the decrypted elf entry */
	jump_to_with_stack(entry, stack);
}
