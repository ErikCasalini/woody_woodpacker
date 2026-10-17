#ifndef SYSCALLS_H
# define SYSCALLS_H

# include <sys/types.h>
# include <stddef.h>
# include <sys/stat.h>

ssize_t			sys_write(int fd, const void *buf, size_t count);
off_t			sys_lseek(int fd, off_t offset, int whence);
_Noreturn void	sys_exit(int status);
void			*sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int				sys_munmap(void *addr, size_t length);
int				sys_mprotect(void *addr, size_t size, int prot);
int				sys_open(const char *path, int flags, ...);
int				sys_close(int fd);
int				sys_fstat(int fd, struct stat *buf);

#endif
