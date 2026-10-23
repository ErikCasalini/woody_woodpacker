#ifndef PACKER_FILE_H
# define PACKER_FILE_H

# include <sys/stat.h>
# include "error.h"

typedef struct mapped_file_s
{
	void		*data;
	struct stat st;
} mapped_file_t;

error_t	map_file(const char *path, mapped_file_t *file);

#endif
