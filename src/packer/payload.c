#include <stdlib.h>
#include "payload.h"
#include "error.h"
#include "file.h"
#include "key.h"
#include "../common/chacha20.h"
#include "../ft/libft.h"
#include "../common/lzss.h"

static void	init_payload_header(payload_header_t *payload_header, size_t packed_sz, size_t original_sz)
{
	ft_memcpy(payload_header->magic, PAYLOAD_MAGIC, sizeof(PAYLOAD_MAGIC));
	payload_header->packed_size	= packed_sz;
	payload_header->original_size = original_sz;
	payload_header->offset = sizeof(payload_header_t);
}

error_t	pack_payload(packed_payload_t *payload, mapped_file_t *target)
{
	error_t	error;
	size_t	packed_sz;

	error = generate_key(payload->key);
	if (error != ok)
		return (error);

	payload->data = malloc((size_t)target->st.st_size);
	if (payload->data == NULL)
		return (bad_malloc);

	ft_memcpy(payload->data, target->data, (size_t)target->st.st_size);

	packed_sz = lzss_compress(target->data, (size_t)target->st.st_size,
								  payload->data, (size_t)target->st.st_size);
	/* If compression fail or is not worth it, we skip it */
	if (packed_sz == LZSS_ERROR)
		packed_sz = target->st.st_size;
	else
		payload->header.is_compressed = 1;

	chacha20_crypt(payload->key, payload->nonce, 0, payload->data, packed_sz);
	init_payload_header(&payload->header, packed_sz, (size_t)target->st.st_size);

	return (ok);
}
