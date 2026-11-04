#include <stdlib.h>
#include "payload.h"
#include "error.h"
#include "file.h"
#include "key.h"
#include "../common/chacha20.h"
#include "../ft/libft.h"
#include "../common/lzss.h"

static void	init_payload_header(payload_header_t *payload_header, size_t compressed_sz, size_t decompressed_sz)
{
	ft_memcpy(payload_header->magic, PAYLOAD_MAGIC, sizeof(PAYLOAD_MAGIC));
	payload_header->compressed_size	= compressed_sz;
	payload_header->decompressed_size = decompressed_sz;
	payload_header->offset = sizeof(payload_header_t);
}

error_t	pack_payload(packed_payload_t *payload, mapped_file_t *target)
{
	error_t	error;
	size_t	compressed_sz;

	error = generate_key(payload->key);
	if (error != ok)
		return (error);

	payload->data = malloc((size_t)target->st.st_size);
	if (payload->data == NULL)
		return (bad_malloc);

	ft_memcpy(payload->data, target->data, (size_t)target->st.st_size);

	compressed_sz = lzss_compress(target->data, (size_t)target->st.st_size,
								  payload->data, (size_t)target->st.st_size);
	if (compressed_sz == LZSS_ERROR) {
		free(payload->data);
		payload->data = NULL;
		return (bad_compress);
	}

	chacha20_crypt(payload->key, payload->nonce, 0, payload->data, compressed_sz);
	init_payload_header(&payload->header, compressed_sz, (size_t)target->st.st_size);

	return (ok);
}
