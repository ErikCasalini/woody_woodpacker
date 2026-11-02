#ifndef STUB_PAYLOAD_H
# define STUB_PAYLOAD_H

# include <stddef.h>
# include <stdint.h>
# include "stub_info.h"
# include "../common/payload.h"

payload_header_t	*find_elf_payload_header(const stub_info_t *stub);
uint8_t			*decrypt_payload(payload_header_t *payload_header);
Elf64_Ehdr			*decompress_payload(const payload_header_t *payload_header,
						uint8_t *compressed, size_t page_size);

#endif
