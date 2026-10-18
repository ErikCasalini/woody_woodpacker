#ifndef ERRORS_H
# define ERRORS_H

typedef enum e_error
{
	ok,
	bad_file,
	bad_interpreter,
	bad_alloc,
	bad_mmap,
	bad_magic,
	bad_endiannes,
	bad_machine,
	no_program_headers,
	bad_program_header,
	bad_dynamic
} t_error;

#endif
