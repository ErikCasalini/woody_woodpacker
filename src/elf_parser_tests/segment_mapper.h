#ifndef SEGMENT_MAPPER_H
# define SEGMENT_MAPPER_H

// # define _DEFAULT_SOURCE // Pour enlever erreur du serveur lsp
# include "errors.h"
# include "elf_types.h"
# include <stddef.h>

# define PIE_LOAD_ADDRESS 0x42000000
# define INTERP_PIE_LOAD_ADDRESS 0x84000000

typedef struct s_segment_map {
	t_elf_program_type	type;
	size_t				vaddr;

} t_segment_map;

t_error	map_segments(uint64_t base_addr, int file, t_ELF64_program_header *program_header, uint32_t segment_num);

#endif

