#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	char byte;
	ssize_t result;

	puts("Waiting for stdin (press Enter to exit)...");
	fflush(stdout);
	do {
		result = read(STDIN_FILENO, &byte, 1);
	} while (result < 0 && errno == EINTR);
	if (result < 0) {
		perror("read");
		return (1);
	}
	return (0);
}
