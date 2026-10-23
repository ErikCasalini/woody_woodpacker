#ifndef PACKER_STUB_SECTION_H
# define PACKER_STUB_SECTION_H

# include <elf.h>
# include "file.h"

const Elf64_Shdr	*find_packer_section(const mapped_file_t *packer, const char *name);

#endif
