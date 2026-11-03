#include "key.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "../common/chacha20.h"

error_t	generate_key(uint8_t *key)
{
	size_t        total;
	ssize_t       n;
	int           fd;
	int           errno_backup;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return (bad_open);
	total = 0;
	while (total < CHACHA20_KEY_SIZE) {
		n = read(fd, key + total, CHACHA20_KEY_SIZE - total);
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

void	print_key(const uint8_t *key)
{
	printf("KEY: 0x");
	for (size_t i = 0; i < CHACHA20_KEY_SIZE; i++)
		printf("%02x", key[i]);
	printf("\n");
}
