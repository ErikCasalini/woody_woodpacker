#ifndef STUB_MESSAGES_H
# define STUB_MESSAGES_H

typedef enum str_code_e
{
	ok,
	bad_stack,
	bad_auxv,
	payload_not_found,
	bad_e_type,
	corrupted,
	bad_load,
	bad_interp,
	start_msg
} str_code_t;

void	print_start_msg(void);
void	print_error(str_code_t code);

#endif
