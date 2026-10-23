#ifndef PACKER_VALIDATE_H
# define PACKER_VALIDATE_H

# include <stdint.h>
# include "file.h"

error_t	check_elf_validity(const mapped_file_t *file, uint64_t page_size);

#endif
