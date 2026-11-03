#ifndef STUB_MESSAGES_H
# define STUB_MESSAGES_H

typedef enum stub_error_e
{
	ok,
	bad_stack,
	bad_auxv,
	payload_not_found,
	bad_e_type,
	corrupted,
	bad_load,
	bad_mmap,
	bad_interp,
	bad_munmap,
	bad_key,
	bad_decompr,
	unexpected,
	bad_mprotect,
	bad_open,
	bad_fstat,
	bad_close
} stub_error_t;

void	print_start_msg(void);
void	print_error(stub_error_t code);

#endif
