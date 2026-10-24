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
	bad_interp,
	bad_munmap
} stub_error_t;

void	print_start_msg(void);
void	print_error(stub_error_t code);

#endif
