#include "interpreter.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "loader.h"
#include "../ft/libft.h"
#include "syscalls/syscalls.h"

stub_error_t	find_interpreter(char *interp_name, const elf_info_t *elf)
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
				return (bad_interp);
			memcpy(interp_name, interp, interp_sz);
			break;
		}
	}
	return (ok);
}

/* We ignore any PT_INTERP in the interpreter binary */
stub_error_t	load_interpreter(elf_info_t *interp, const char *interp_name, size_t page_size)
{
	int			interp_fd;
	struct stat	st;
	stub_error_t	error;

	interp_fd = sys_open(interp_name, O_RDONLY);
	if (interp_fd < 0)
		return (bad_open);

	if (sys_fstat(interp_fd, &st) < 0) {
		error = bad_fstat;
		goto close_fd;
	}

	interp->file_sz = (size_t)st.st_size;
	interp->file_header = (Elf64_Ehdr*) sys_mmap(NULL,
										interp->file_sz,
										PROT_READ,
										MAP_PRIVATE,
										interp_fd,
										0);
	if ((intptr_t)interp->file_header < 0) {
		error = bad_mmap;
		goto close_fd;
	}

	interp->file_phdr = (Elf64_Phdr*)((unsigned char*)interp->file_header +
		interp->file_header->e_phoff
	);

	/* For now, due to the complexity of preventing corruption of the stub
	pages caused by a mapping collision with an ET_EXEC interpreter,
	we reject this type of interpreters */
	if (interp->file_header->e_type == ET_DYN) {
		error = map_ET_DYN(interp, page_size);
		if (error != ok)
			goto free_interp_load_zone;
	}
	else {
		error = bad_e_type;
		goto free_interp_load_zone;
	}

	if (sys_close(interp_fd) < 0)
		return (bad_close);
	return (ok);

free_interp_load_zone:
	sys_munmap(interp->file_header, interp->file_sz);
close_fd:
	sys_close(interp_fd);
	return (error);
}
