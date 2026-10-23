#ifndef PACKER_DEBUG_H
# define PACKER_DEBUG_H

# include "layout.h"

#ifdef DEBUG
void	print_output_layout(const elf_layout_t *output);
#endif

#endif
