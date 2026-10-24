#ifndef STUB_PAYLOAD_H
# define STUB_PAYLOAD_H

# include "stub_info.h"
# include "../common/payload.h"

payload_header_t	*find_elf_payload_header(const stub_info_t *stub);
Elf64_Ehdr			*decrypt_payload(payload_header_t *payload_header);

#endif
