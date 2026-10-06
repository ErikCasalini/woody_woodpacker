/* packer assumes :
-Interpreter must be a valid ELF64 ET_DYN for x86-64, little-endian, with p_align <= runtime page_size for every PT_LOAD.

packer checks :
-Elf must be x86_64
-Must have an Header
-Must have a PHDR
-Its PT_LOAD must be in ascending order
-Must not have s_isuid set in permissions
-Must be executable
-Must be ET_DYN or ET_EXEC
-Must be little endian
-If ELF is ET_EXEC, its load address must not be < 0x10000 or > (0xMAX - memsize of all PT_LOADS)
-Each PT_LOAD must have p_align equal to 0, 1, or a power of two.
-If p_align > 1, p_vaddr % p_align must equal p_offset % p_align.
-For ET_DYN, each PT_LOAD must have p_align <= runtime page_size.
-If INTERP present, PT_INTERP filesz must be > 1
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <elf.h>
#include <sys/mman.h>
#include <sys/stat.h>


typedef enum error_e
{
	ok,

} error_t;


error_t	check_elf_validity(void *elf)
{


	return (ok);
}

int main(int argc, char *argv[])
{
	int			elf_fd;
	void		*elf_file;
	struct stat	elf_st;

	if (argc < 2)
		return (printf("Specify a filename\n"), 1);

	elf_fd = open(argv[1], O_RDONLY);
	if (elf_fd < 0)
		return (printf("Error: open() %s\n", strerror(errno)), 1);

	if (fstat(elf_fd, &elf_st) < 0)
		return (printf("Error: fstat() %s\n", strerror(errno)), 1);

	elf_file = mmap(NULL, elf_st.st_size, PROT_READ, MAP_PRIVATE, elf_fd, 0);
	if (elf_file == NULL)
		return (printf("Error: mmap() %s\n", strerror(errno)), 1);

	error = parse_ELF_header(file, &elf);
	if (error != ok) {
		close(file);
		print_error(error);
		return (1);
	}
}
