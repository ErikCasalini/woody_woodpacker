#include <stddef.h>
#include "../common/elf_range.h"
#include "../common/page.h"
#include "auxv.h"
#include "interpreter.h"
#include "jump_to_with_stack.h"
#include "loader.h"
#include "messages.h"
#include "payload.h"
#include "stub_info.h"
#include "syscalls/syscalls.h"

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

static int	munmap_files(elf_info_t *interp, payload_header_t *payload_header, size_t page_size)
{
	int				error = 0;
	mapping_range_t	elf_file_range;
	mapping_range_t	interp_file_range;

	if (interp && interp->file_header) {
		interp_file_range.start = round_to_page_down((uintptr_t)interp->file_header, page_size);
		interp_file_range.end = round_to_page_up((uintptr_t)interp->file_header + interp->file_sz, page_size);
		if (sys_munmap((void*)interp_file_range.start, interp_file_range.end - interp_file_range.start) < 0)
			error = -1;
	}

	if (payload_header) {
		elf_file_range.start = round_to_page_down((uintptr_t)payload_header, page_size);
		elf_file_range.end = round_to_page_up((uintptr_t)payload_header + payload_header->offset + payload_header->compressed_size, page_size);
		if (sys_munmap((void*)elf_file_range.start, elf_file_range.end - elf_file_range.start) < 0)
			error = -1;
	}

	return (error);
}

_Noreturn void	execute_packed_elf(uintptr_t *stack)
{
	stub_info_t			stub = {0};						/* Refers THIS binary (packed elf loaded by the kernel)  */
	elf_info_t			elf = {0};						/* Refers to the ORIGINAL elf encrypted inside THIS binary as a payload */
	elf_info_t			interp = {0};					/* Refers to the interpreter of the ORIGINAL elf */
	Elf64_auxv_t		*auxv;							/* Will contain the begin of the auxv table */
	payload_header_t	*payload_header;				/* Will contain the address of the ORIGINAL elf payload header */
	char				interp_name[PATH_MAX] = {0};	/* Will contain the string of the ORIGINAL elf interpreter's name */
	void				*entry = NULL;					/* Will contain the entry address of the ORIGINAL elf or its interpreter's */
	size_t				page_size;						/* Will contain the page size for this execution */
	int					retval;
	Elf64_auxv_t		*auxv_field;

	print_start_msg();

	if (!stack)
		fail(bad_stack);

	auxv = find_auxv_start(stack);

	auxv_field = find_auxv_field(auxv, AT_PAGESZ);
	if (auxv_field == NULL)
		fail(bad_auxv);
	page_size = auxv_field->a_un.a_val;

	retval = init_stub_info(&stub, auxv);
	if (retval != ok)
		fail(retval);

	payload_header = find_elf_payload_header(&stub);
	if (payload_header == NULL)
		fail(payload_not_found);

	elf.file_header = decrypt_payload(payload_header);
	if (elf.file_header == NULL)
		fail(corrupted);
	elf.file_sz = payload_header->decompressed_size;
	elf.file_phdr = (Elf64_Phdr *)((unsigned char*)elf.file_header +
					elf.file_header->e_phoff);

	if (elf.file_header->e_type == ET_EXEC)
		retval = map_ET_EXEC(&elf, page_size);
	else if (elf.file_header->e_type == ET_DYN)
		retval = map_ET_DYN(&elf, page_size);
	else
		fail(bad_e_type);

	if (retval < 0)
		fail(bad_load);

	if (find_interpreter(interp_name, &elf) < 0)
		fail(bad_interp);

	/* This stub assumes that interpreter is a valid ELF */
	if (interp_name[0] != '\0') {
		if (load_interpreter(&interp, interp_name, page_size) < 0)
			fail(bad_interp);
		entry = (void*)(interp.file_header->e_entry + interp.load_bias);
	}
	else
		/* ORIGINAL ELF e_entry must be checked by the packer */
		entry = (void*)(elf.file_header->e_entry + elf.load_bias);

	/* We need to update auxv data to make it match the original elf configuration.
	It will be used by the interpreter to set the elf up */
	if (fix_auxv(auxv, &elf, &interp) < 0)
		fail(bad_auxv);

	if (munmap_files(&interp, payload_header, page_size) < 0)
		fail(bad_munmap);

	/* We jump to the loaded interepreter or directly to the decrypted elf entry */
	jump_to_with_stack(entry, stack);
}
