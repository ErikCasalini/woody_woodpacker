#ifndef PACKER_PAYLOAD_H
# define PACKER_PAYLOAD_H

# include "file.h"
# include "../common/payload.h"

error_t	pack_payload(packed_payload_t *payload, mapped_file_t *target);

#endif
