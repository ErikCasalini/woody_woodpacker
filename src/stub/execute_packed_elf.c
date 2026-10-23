#include <stddef.h>
#include "auxv.h"
#include "interpreter.h"
#include "jump_to_with_stack.h"
#include "loader.h"
#include "messages.h"
#include "payload.h"
#include "syscalls/syscalls.h"

#define EXIT_FAILURE 1

static str_code_t	init_this_elf_struct(elf_info_t *this, Elf64_auxv_t *auxv)
{
	Elf64_auxv_t *auxv_field;

	auxv_field = find_auxv_field(auxv, AT_PHDR);
	if (auxv_field == NULL)
		return (bad_auxv);
	this->mem_phdr = (Elf64_Phdr*)auxv_field->a_un.a_val;

	/* The stub assumes that the packer stores the ELF Header at
	the very beginning of the binary, followed by the ELF Phdr,
	and that they will be loaded in this order in the same PT_LOAD */
	this->mem_header = (Elf64_Ehdr*)((unsigned char*)this->mem_phdr - sizeof(Elf64_Ehdr));

	/* The stub assumes thats its e_type is the same of the original elf */
	if (this->mem_header->e_type == ET_EXEC)
		this->load_bias = 0;
	else if (this->mem_header->e_type == ET_DYN)
		this->load_bias = (uint64_t)this->mem_header;
	else
		return (bad_e_type);

	return (ok);
}

_Noreturn void	execute_packed_elf(uintptr_t *stack)
{
	elf_info_t			this = {0};						/* refers to THIS running elf, the binary resulting of a packing, which entry is this stub. */
	elf_info_t			elf = {0};						/* refers to the ORIGINAL elf encrypted inside THIS binary as a payload. */
	elf_info_t			interp = {0};					/* refers to the interpreter of the ORIGINAL elf */
	Elf64_auxv_t		*auxv;							/* Will contain the begin of the auxv table */
	payload_header_t	*payload_header;				/* Will contain the address of the ORIGINAL elf payload header */
	char				interp_name[PATH_MAX] = {0};	/* Will contain the string of the ORIGINAL elf interpreter's name */
	void				*entry = NULL;					/* Will contain the entry address of the ORIGINAL elf or its interpreter's */
	uint64_t			page_size;						/* Will contain the page size for this execution */
	int					retval;
	Elf64_auxv_t		*auxv_field;

	print_start_msg();

	if (!stack) {
		print_error(bad_stack);
		sys_exit(EXIT_FAILURE);
	}

	auxv = find_auxv_start(stack);

	auxv_field = find_auxv_field(auxv, AT_PAGESZ);
	if (auxv_field == NULL) {
		print_error(bad_auxv);
		sys_exit(EXIT_FAILURE);
	}
	page_size = auxv_field->a_un.a_val;

	retval = init_this_elf_struct(&this, auxv);
	if (retval != ok) {
		print_error(retval);
		sys_exit(EXIT_FAILURE);
	}

	payload_header = find_elf_payload_header(&this);
	if (payload_header == NULL) {
		print_error(payload_not_found);
		sys_exit(EXIT_FAILURE);
	}

	elf.file_header = decrypt_payload(payload_header); // a faire
	if (elf.file_header == NULL) {
		print_error(corrupted);
		sys_exit(EXIT_FAILURE);
	}
	elf.file_data_sz = payload_header->decompressed_size;
	elf.file_phdr = (Elf64_Phdr *)((unsigned char*)elf.file_header +
					elf.file_header->e_phoff);

	if (elf.file_header->e_type == ET_EXEC)
		retval = map_ET_EXEC(&elf, page_size);
	else if (elf.file_header->e_type == ET_DYN)
		retval = map_ET_DYN(&elf, page_size);
	else {
		print_error(bad_e_type);
		sys_exit(EXIT_FAILURE);
	}

	if (retval < 0) {
		print_error(bad_load);
		sys_exit(EXIT_FAILURE);
	}

	if (find_interpreter(interp_name, &elf) < 0) {
		print_error(bad_interp);
		sys_exit(EXIT_FAILURE);
	}

	/* This stub assumes that interpreter is a valid ELF */
	if (interp_name[0] != '\0') {
		if (load_interpreter(&interp, interp_name, page_size) < 0) {
			print_error(bad_interp);
			sys_exit(EXIT_FAILURE);
		}
		entry = (void*)(interp.file_header->e_entry + interp.load_bias);
	}
	else
		/* ORIGINAL ELF e_entry must be checked by the packer */
		entry = (void*)(elf.file_header->e_entry + elf.load_bias);

	/* We need to update auxv data to make it match the original elf configuration.
	It will be used by the interpreter to set the elf up */
	if (fix_auxv(auxv, &elf, &interp) < 0) {
		print_error(bad_auxv);
		sys_exit(EXIT_FAILURE);
	}

	sys_munmap(interp.file_header, interp.file_data_sz);
	// sys_munmap(payload_header, payload_header->decompressed_size); Si on veut free il faut calculer la base de la page et sa fin

	/* We jump to the loaded interepreter or directly to the decrypted elf entry */
	jump_to_with_stack(entry, stack);
}
