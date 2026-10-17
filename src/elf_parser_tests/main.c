#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "elf_types.h"
#include "errors.h"
#include "str_enum.h"
#include "segment_mapper.h"
#include "pt_dynamic.h"
#include "stack_fix.h"
#include "jump_to_with_stack.h"

#include <stddef.h>

_Static_assert(sizeof(t_ELF64_header) == 64, "t_ELF64_header must have size of 64 bytes");

typedef void (*_entry)(void);

void	print_error(t_error error)
{
	switch(error)
	{
		case ok:
			return;
		case bad_file:
			printf("Error during operation of file : %s\n", strerror(errno));
			break;
		case bad_interpreter:
			printf("Error during operation of interpreter file : %s\n", strerror(errno));
			break;
		case bad_alloc:
			printf("Error during memory allocation : %s\n", strerror(errno));
			break;
		case bad_mmap:
			printf("Error during memory mapping : %s\n", strerror(errno));
			break;
		case bad_machine:
			printf("Unsupported binary architecture\n");
			break;
		case bad_endiannes:
			printf("Big endian binaires are not supported\n");
			break;
		case bad_magic:
			printf("Unsupported file type\n");
			break;
		case no_program_headers:
			printf("Invalid executable : No segments found\n");
			break;
		case bad_program_header:
			printf("Invalid program header\n");
			break;
		case bad_dynamic:
			printf("Invalid Dynamic segment\n");
			break;
		default:
			printf("Unkonw error\n");
	}
}

void	print_ELF_header_fields(t_ELF64 elf)
{
	printf("------------------------HEADER-------------------------\n");
	printf("|%-34s %02x %02x %02x %02x       |\n", "Magic:",
		elf.header.ident.magic[0],
		elf.header.ident.magic[1],
		elf.header.ident.magic[2],
		elf.header.ident.magic[3]);
	printf("|%-34s %-18s|\n", "Class:",
		str_class((t_elf_class)elf.header.ident.elf_class));
	printf("|%-34s %-18s|\n", "Data:",
		str_data((t_elf_data)elf.header.ident.data));
	printf("|%-34s %-18s|\n", "Version:",
		str_version((t_elf_version)elf.header.ident.version));
	printf("|%-34s %-18s|\n", "OS ABI:",
		str_os_abi((t_elf_os_abi)elf.header.ident.os_abi));
	printf("|%-34s %-18u|\n", "ABI version:",
		(unsigned int)elf.header.ident.abi_version);
	printf("|%-34s %-18s|\n", "Type:",
		str_type((t_elf_type)elf.header.type));
	printf("|%-34s %-18s|\n", "Machine:",
		str_machine((t_elf_machine)elf.header.machine));
	printf("|%-34s %-18s|\n", "ELF version:",
		str_version((t_elf_version)elf.header.version));
	printf("|%-34s 0x%016lx|\n", "Entry address:",
		(unsigned long)elf.header.entry);
	printf("|%-34s %-10lu (bytes)|\n", "Program header offset:",
		(unsigned long)elf.header.program_header_offset);
	printf("|%-34s %-10lu (bytes)|\n", "Section header offset:",
		(unsigned long)elf.header.section_header_offset);
	printf("|%-34s 0x%-16x|\n", "Flags:", (unsigned int)elf.header.flags);
	printf("|%-34s %-10u (bytes)|\n", "ELF header size:",
		(unsigned int)elf.header.header_size);
	printf("|%-34s %-10u (bytes)|\n", "Program header entry size:",
		(unsigned int)elf.header.program_header_entry_size);
	printf("|%-34s %-18u|\n", "Program header count:",
		(unsigned int)elf.header.program_header_number);
	printf("|%-34s %-10u (bytes)|\n", "Section header entry size:",
		(unsigned int)elf.header.section_header_entry_size);
	printf("|%-34s %-18u|\n", "Section header count:",
		(unsigned int)elf.header.section_header_number);
	printf("|%-34s %-18u|\n", "Section name table index:",
		(unsigned int)elf.header.section_header_string_index);
}

void print_ELF_program_header(t_ELF64 elf)
{
	printf("-------------------------------------------------------PROGRAM HEADER--------------------------------------------------------\n");
	for (int i = 0; i < elf.header.program_header_number; i++) {
		printf("|Type: %-18s |File Offset: %#-10lx|File Size: %#-10lx|Vaddr: %016lx   |Mem Size: %#-8lx |%s |\n",
			str_program_type((t_elf_program_type)elf.program_header[i].type),
			elf.program_header[i].offset,
			elf.program_header[i].filesz,
			elf.program_header[i].vaddr,
			elf.program_header[i].memsz,
			str_program_flags(elf.program_header[i].flags));
	}
	printf("-----------------------------------------------------------------------------------------------------------------------------\n");
}

char *find_elf_field_in_memory(void *field_addr, uint64_t size)
{
	char *field = NULL;

	field =	malloc(size);
	if (field == NULL)
		return (NULL);

	memcpy(field, field_addr, size);

	return (field);
}

char	*find_interperter(t_ELF64 elf, uint64_t base_addr)
{ // IL FAUT CHERCHER INTERP DANS LE FICHIER PAS DANS LA MEMOIRE
	char * interprer = NULL;

	if (elf.program_header == NULL)
		return (NULL);

	for (int i = 0; i < elf.header.program_header_number; i++) {
		if (elf.program_header[i].type == elf_program_type_interp) {
			if (elf.header.type == elf_type_dynamic)
				interprer = find_elf_field_in_memory((void *)elf.program_header[i].vaddr + base_addr, elf.program_header[i].memsz);
			else if (elf.header.type == elf_type_executable)
				interprer = find_elf_field_in_memory((void *)elf.program_header[i].vaddr, elf.program_header[i].memsz);
			else
				return (printf("Unsupported ELF type\n"), NULL);
			break;
		}
	}
	return (interprer);
}

t_error	parse_ELF_header(int file, t_ELF64 *elf)
{
	if (read(file, &elf->header, sizeof(t_ELF64_header))
		!= (ssize_t)sizeof(t_ELF64_header))
		return (bad_file);
	if (elf->header.ident.data != elf_data_little_endian)
		return (bad_endiannes);
	return (ok);
}

t_error parse_ELF_program_header(int file, t_ELF64 *elf)
{
	if (!elf->header.program_header_number)
		return (no_program_headers);
	//METTRE CELUI DE LA LIBFT
	elf->program_header = calloc(elf->header.program_header_number, sizeof(t_ELF64_program_header));
	if (elf->program_header == NULL)
		return (bad_alloc);

	if (lseek(file, elf->header.program_header_offset, SEEK_SET) < 0)
		return (free(elf->program_header), bad_program_header);

	for (int i = 0; i < elf->header.program_header_number; i++) {
		if (read(file, &elf->program_header[i], sizeof(t_ELF64_program_header)) != sizeof(t_ELF64_program_header))
			return (free(elf->program_header), bad_program_header);
	}
	return (ok);
}

int	main(int argc, char *argv[])
{
	int		file;
	int		interpreter_fd;
	t_error	error;
	t_ELF64	elf;
	t_ELF64 interpreter;
	char	*elf_interpreter;
	uint64_t interp_base_addr = 0x0;
	uint64_t entry;

	if (argc < 2)
		return (printf("Specify a filename\n"), 1);

	file = open(argv[1], O_RDONLY);
	if (file < 0)
		return (print_error(bad_file), 1);

	elf.filename = argv[1];
	error = parse_ELF_header(file, &elf);
	if (error != ok) {
		close(file);
		print_error(error);
		return (1);
	}
	error = parse_ELF_program_header(file, &elf);
	if (error != ok) {
		print_error(error);
		close(file);
		return (1);
	}
	print_ELF_header_fields(elf);
	print_ELF_program_header(elf);

	uint64_t base_addr;

	if (elf.header.type == elf_type_dynamic)
		base_addr = PIE_LOAD_ADDRESS;
	else
		base_addr = 0x0;

	error = map_segments(base_addr, file, elf.program_header, elf.header.program_header_number);
	if (error != ok) {
		print_error(error);
		close(file);
		return (1);
	}

	///////////////// INTERPRETER /////////////////
	/* AJOUTER PROTECTION SI INTERP != ld-linux-x86-64.so.2 */

	elf_interpreter = find_interperter(elf, base_addr);
	if (elf_interpreter == NULL) {
		printf("No interpreter found\n");
		goto exec_elf;
	}
	else {
		printf("Interpreter: %s\n", elf_interpreter);
	}

	interpreter_fd = open(elf_interpreter, O_RDONLY);
	if (interpreter_fd < 0)
		return (print_error(bad_interpreter), 1);

	error = parse_ELF_header(interpreter_fd, &interpreter);
	if (error != ok) {
		close(interpreter_fd);
		print_error(error);
		return (1);
	}
	error = parse_ELF_program_header(interpreter_fd, &interpreter);
	if (error != ok) {
		print_error(error);
		close(interpreter_fd);
		return (1);
	}
	print_ELF_header_fields(interpreter);
	print_ELF_program_header(interpreter);

	if (interpreter.header.type == elf_type_dynamic)
		interp_base_addr = INTERP_PIE_LOAD_ADDRESS;
	else
		interp_base_addr = 0x0;

	error = map_segments(interp_base_addr, interpreter_fd, interpreter.program_header, interpreter.header.program_header_number);
	if (error != ok) {
		print_error(error);
		close(file);
		return (1);
	}

	exec_elf:
	update_stack(&elf, (uint8_t**)argv - 1, base_addr, interp_base_addr);
	//////////////////////////////////////////////

	// error = print_relocations_fields(file, elf, base_addr);
	// if (error != ok) {
	// 	print_error(error);
	// 	close(file);
	// 	return (1);
	// }

	printf("Jumping to loaded entry point...\n\n");
	// _entry entry = (_entry)(interpreter.header.entry + interp_base_addr);
	// entry();

	if (elf_interpreter)
		entry = interpreter.header.entry + interp_base_addr;
	else {
		entry = elf.header.entry + base_addr;
	}
	jump_to_with_stack((void*)entry, (void*)(argv - 1));

	close(file);
	printf("\nDONE\n");
	return (0);
}
