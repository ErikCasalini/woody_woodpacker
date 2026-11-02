#ifndef PACKER_OUTPUT_H
# define PACKER_OUTPUT_H

# include "layout.h"
# include "../common/payload.h"

# define OUTPUT_FILE "woody"

int		create_output_file(const char *path, const struct stat *target_st);
void	init_payload_header(payload_header_t *payload_header, const mapped_file_t *target);
error_t	generate_payload_key(payload_header_t *payload_header);
error_t	fill_output_elf(int						output_fd,
						const elf_layout_t		*output,
						const Elf64_Shdr		*packer_stub_section,
						const Elf64_Ehdr		*packer_file_data,
						const Elf64_Ehdr		*target_file_data,
						const payload_header_t	*payload_header);

#endif
