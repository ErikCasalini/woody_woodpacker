#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "output.h"
#include "payload.h"

int	create_output_file(const char *path, const struct stat *target_st)
{
	int			fd;
	int			errno_backup;
	struct stat	output_st;
	/* We avoid O_TRUNC in case target and output are
	the same file */
	fd = open(path, O_CREAT | O_WRONLY, 0755);
	if (fd < 0)
		return (-1);

	if (fstat(fd, &output_st) < 0)
		goto cleanup;
	/* We ensure target and output files are not the same */
	if (target_st->st_dev == output_st.st_dev
		&& target_st->st_ino == output_st.st_ino) {
		errno = EINVAL;
		goto cleanup;
	}
	/* We need to manually truncate a potentially already existent
	output file */
	if (ftruncate(fd, 0) < 0)
		goto cleanup;
	return (fd);

cleanup:
	errno_backup = errno;
	close(fd);
	errno = errno_backup;
	return (-1);
}

error_t	fill_output_elf(int						output_fd,
						const elf_layout_t		*output,
						const Elf64_Shdr		*packer_stub_section,
						const Elf64_Ehdr		*packer_file_data,
						const packed_payload_t	*payload
						)
{
	const void            *start_stub_section;
	ssize_t               written;

	start_stub_section = (const unsigned char*)packer_file_data + packer_stub_section->sh_offset;

	written = write(output_fd, &output->header, sizeof(output->header));
	if (written != (ssize_t)sizeof(output->header))
		return (write_error);

	written = write(output_fd, &output->phdrs_segment, sizeof(output->phdrs_segment));
	if (written != (ssize_t)sizeof(output->phdrs_segment))
		return (write_error);

	written = write(output_fd, &output->stub_segment, sizeof(output->stub_segment));
	if (written != (ssize_t)sizeof(output->stub_segment))
		return (write_error);

	written = write(output_fd, &output->payload_segment, sizeof(output->payload_segment));
	if (written != (ssize_t)sizeof(output->payload_segment))
		return (write_error);

	written = write(output_fd, &output->gnu_stack_segment, sizeof(output->gnu_stack_segment));
	if (written != (ssize_t)sizeof(output->gnu_stack_segment))
		return (write_error);

	written = write(output_fd, start_stub_section, packer_stub_section->sh_size);
	if (written != (ssize_t)packer_stub_section->sh_size)
		return (write_error);

	written = write(output_fd, &payload->header, sizeof(payload_header_t));
	if (written != (ssize_t)sizeof(payload_header_t))
		return (write_error);

	written = write(output_fd, payload->data, payload->header.packed_size);
	if (written != (ssize_t)payload->header.packed_size)
		return (write_error);

	return (ok);
}
