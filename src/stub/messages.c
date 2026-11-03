#include "messages.h"
#include "../ft/libft.h"
#include "syscalls/syscalls.h"

#define STDOUT 1
#define STDERR 2
#define ERR_LEN 468

#define START_MESSAGE \
    "+------------------------------------------------+\n" \
    "|  __        __   ___     ___    ____   __   __  |\n" \
    "|  \\ \\      / /  / _ \\   / _ \\  |  _ \\  \\ \\ / /  |\n" \
    "|   \\ \\ /\\ / /  | | | | | | | | | | | |  \\ V /   |\n" \
    "|    \\ V  V /   | |_| | | |_| | | |_| |   | |    |\n" \
    "|     \\_/\\_/     \\___/   \\___/  |____/    |_|    |\n" \
    "|                                                |\n" \
    "+------------------------------------------------+\n"

static const char	error_messages[][ERR_LEN] = {
	[bad_stack] = "Invalid initial stack\n",
	[bad_auxv] = "Invalid or incomplete auxiliary vector\n",
	[payload_not_found] = "Payload not found or invalid\n",
	[bad_e_type] = "Unsupported ELF type\n",
	[corrupted] = "Payload or unpacked ELF is corrupted\n",
	[bad_load] = "Unable to prepare ELF mappings: address range is too large\n",
	[bad_mmap] = "Unable to map memory\n",
	[bad_interp] = "Invalid ELF interpreter path\n",
	[bad_munmap] = "Unable to unmap memory\n",
	[bad_key] = "Invalid key: expected 0x followed by 64 hexadecimal digits\n",
	[bad_decompr] = "Unable to decompress payload\n",
	[unexpected] = "Unexpected error\n",
	[bad_mprotect] = "Unable to set memory protections\n",
	[bad_open] = "Unable to open ELF interpreter\n",
	[bad_fstat] = "Unable to read ELF interpreter file information\n",
	[bad_close] = "Unable to close ELF interpreter file\n",
};

void	print_start_msg(void)
{
	sys_write(STDOUT, START_MESSAGE, sizeof(START_MESSAGE) - 1);
}

void	print_error(stub_error_t code)
{
	sys_write(STDERR, error_messages[code], ft_strlen(error_messages[code]));
}
