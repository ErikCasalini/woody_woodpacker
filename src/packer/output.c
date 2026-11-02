#include "output.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "../ft/libft.h"
#include "../common/chacha20.h"
#include "../common/lzss.h"

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

void	init_payload_header(payload_header_t *payload_header, const mapped_file_t *target, size_t compressed_size)
{
	ft_memcpy(payload_header->magic, PAYLOAD_MAGIC, sizeof(PAYLOAD_MAGIC));
	payload_header->compressed_size = compressed_size;
	payload_header->decompressed_size = (size_t)target->st.st_size;
	payload_header->offset = sizeof(payload_header_t);
}

error_t	build_payload(const mapped_file_t	*target,
		const uint8_t			key[CHACHA20_KEY_SIZE],
		unsigned char			**out_buf,
		size_t				*out_size)
{
	static const uint8_t	nonce[CHACHA20_NONCE_SIZE] = {0};
	size_t			decompressed_size;
	size_t			compressed_size;
	unsigned char		*buf;

	decompressed_size = (size_t)target->st.st_size;
	buf = malloc(decompressed_size ? decompressed_size : 1);
	if (buf == NULL)
		return (write_error);
	compressed_size = lzss_compress((const uint8_t *)target->data,
			decompressed_size, buf, decompressed_size);
	if (compressed_size == LZSS_ERROR || compressed_size >= decompressed_size) {
		ft_memcpy(buf, target->data, decompressed_size);
		compressed_size = decompressed_size;
	}
	chacha20_crypt(key, nonce, 0, buf, compressed_size);
	*out_buf = buf;
	*out_size = compressed_size;
	return (ok);
}

error_t	generate_payload_key(payload_header_t *payload_header)
{
	unsigned char *key;
	size_t        total;
	ssize_t       n;
	int           fd;
	int           errno_backup;

	key = payload_header->key;
	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return (bad_open);
	total = 0;
	while (total < sizeof(payload_header->key)) {
		n = read(fd, key + total, sizeof(payload_header->key) - total);
		if (n <= 0) {
			errno_backup = errno;
			close(fd);
			errno = errno_backup;
			return (bad_read);
		}
		total += (size_t)n;
	}
	if (close(fd) < 0)
		return (bad_close);
	return (ok);
}

error_t	fill_output_elf(int						output_fd,
						const elf_layout_t		*output,
						const Elf64_Shdr		*packer_stub_section,
						const Elf64_Ehdr		*packer_file_data,
						const payload_header_t	*payload_header,
						const unsigned char		*payload_buf,
						size_t					payload_size)
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

	written = write(output_fd, payload_header, sizeof(payload_header_t));
	if (written != (ssize_t)sizeof(payload_header_t))
		return (write_error);

	written = write(output_fd, payload_buf, payload_size);
	if (written != (ssize_t)payload_size)
		return (write_error);

	return (ok);
}
