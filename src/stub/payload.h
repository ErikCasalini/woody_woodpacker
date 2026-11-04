#ifndef STUB_PAYLOAD_H
# define STUB_PAYLOAD_H

# include "stub_info.h"
# include "../common/payload.h"
# include "messages.h"
# include "elf_info.h"

stub_error_t	extract_payload(packed_payload_t *payload, const stub_info_t *stub);
stub_error_t	unpack_payload(packed_payload_t *payload, elf_info_t *elf);

#endif
