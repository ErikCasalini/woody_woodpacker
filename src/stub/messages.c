#include "messages.h"
#include "../ft/libft.h"
#include "syscalls/syscalls.h"

#define STDOUT 1
#define STDERR 2
#define STRINGS_LEN 468

static const char	strings[][STRINGS_LEN] = {
	[start_msg] = "+------------------------------------------------+\n"
    "|  __        __   ___     ___    ____   __   __  |\n"
    "|  \\ \\      / /  / _ \\   / _ \\  |  _ \\  \\ \\ / /  |\n"
    "|   \\ \\ /\\ / /  | | | | | | | | | | | |  \\ V /   |\n"
    "|    \\ V  V /   | |_| | | |_| | | |_| |   | |    |\n"
    "|     \\_/\\_/     \\___/   \\___/  |____/    |_|    |\n"
    "|                                                |\n"
    "+------------------------------------------------+\n",
	[bad_stack] = "execute_packed_elf : illformed stack\n",
	[bad_auxv] = "find_auxv_field : invalid auxv\n",
	[payload_not_found] = "find_elf_payload_header : original elf not found or currupted\n",
	[bad_e_type] = "execute_packed_elf : unsupported ELF type\n",
	[corrupted] = "decrypt_in_place : original elf corrupted\n",
	[bad_load] = "load_elf : error during mapping original elf\n",
	[bad_interp] = "load_interpreter : invalid interpreter or error during interpreter loading\n",
};

void	print_start_msg(void)
{
	sys_write(STDOUT, strings[start_msg], ft_strlen(strings[start_msg]));
}

void	print_error(str_code_t code)
{
	sys_write(STDERR, strings[code], ft_strlen(strings[code]));
}
