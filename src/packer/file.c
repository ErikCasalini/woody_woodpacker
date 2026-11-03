#include "file.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

error_t	map_file(const char *path, mapped_file_t *file)
{
	error_t	error = ok;
	int		fd;
	void	*data;
	int		errno_backup;

	if (!path || !file)
		return (unexpected_error);

	file->data = MAP_FAILED;
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (bad_open);

	if (fstat(fd, &file->st) < 0) {
		error = bad_fstat;
		goto cleanup;
	}

	data = mmap(NULL, file->st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (data == MAP_FAILED) {
		error = bad_mmap;
		goto cleanup;
	}

	if (close(fd) < 0) {
		errno_backup = errno;
		munmap(data, file->st.st_size);
		errno = errno_backup;
		return (bad_close);
	}

	file->data = data;
	return (ok);

cleanup:
	errno_backup = errno;
	close(fd);
	errno = errno_backup;
	return (error);
}
