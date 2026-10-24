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
	[bad_stack] = "execute_packed_elf : illformed stack\n",
	[bad_auxv] = "find_auxv_field : invalid auxv\n",
	[payload_not_found] = "find_elf_payload_header : original elf not found or currupted\n",
	[bad_e_type] = "execute_packed_elf : unsupported ELF type\n",
	[corrupted] = "decrypt_in_place : original elf corrupted\n",
	[bad_load] = "load_elf : error during mapping original elf\n",
	[bad_interp] = "load_interpreter : invalid interpreter or error during interpreter loading\n",
	[bad_munmap] = "munmap_files : error during file image cleanup\n",
};

void	print_start_msg(void)
{
	sys_write(STDOUT, START_MESSAGE, sizeof(START_MESSAGE) - 1);
}

void	print_error(stub_error_t code)
{
	sys_write(STDERR, error_messages[code], ft_strlen(error_messages[code]));
}
