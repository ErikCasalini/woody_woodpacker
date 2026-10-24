#include "interpreter.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "loader.h"
#include "../ft/libft.h"
#include "syscalls/syscalls.h"

int	find_interpreter(char *interp_name, elf_info_t *elf)
{
	char		*interp;
	Elf64_Xword	interp_sz;

	interp_name[0] = '\0';

	for (Elf64_Half i = 0; i < elf->file_header->e_phnum; i++) {
		if (elf->file_phdr[i].p_type == PT_INTERP) {
			interp = (char*)elf->file_header + elf->file_phdr[i].p_offset;
			interp_sz = elf->file_phdr[i].p_filesz;

			if (interp_sz < 2 || interp_sz > PATH_MAX ||
				interp[interp_sz - 1] != '\0')
				return (-1);
			memcpy(interp_name, interp, interp_sz);
			break;
		}
	}
	return (0);
}

/* We ignore any PT_INTERP in the interpreter binary */
int	load_interpreter(elf_info_t *interp, char *interp_name, uint64_t page_size)
{
	int			interp_fd;
	struct stat	st;

	interp_fd = sys_open(interp_name, O_RDONLY);
	if (interp_fd < 0)
		return (-1);

	if (sys_fstat(interp_fd, &st) < 0)
		goto close_fd;

	interp->file_sz = st.st_size;
	interp->file_header = (Elf64_Ehdr*) sys_mmap(NULL,
										interp->file_sz,
										PROT_READ,
										MAP_PRIVATE,
										interp_fd,
										0);
	if ((intptr_t)interp->file_header < 0)
		goto close_fd;

	interp->file_phdr = (Elf64_Phdr*)((unsigned char*)interp->file_header +
		interp->file_header->e_phoff
	);

	/* For now, due to the complexity of preventing corruption of the stub
	pages caused by a mapping collision with an ET_EXEC interpreter,
	we reject this type of interpreters */
	if (interp->file_header->e_type == ET_DYN) {
		if (map_ET_DYN(interp, page_size) < 0)
			goto free_interp_load_zone;
	}
	else
		goto free_interp_load_zone;

	sys_close(interp_fd);
	return (0);

free_interp_load_zone:
	sys_munmap(interp->file_header, interp->file_sz);
close_fd:
	sys_close(interp_fd);
	return (-1);
}
