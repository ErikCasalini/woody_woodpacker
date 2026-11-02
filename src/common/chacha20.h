#ifndef CHACHA20_H
# define CHACHA20_H

# include <stddef.h>
# include <stdint.h>

# define CHACHA20_KEY_SIZE   32
# define CHACHA20_NONCE_SIZE 12

void	chacha20_crypt(const uint8_t key[CHACHA20_KEY_SIZE],
			const uint8_t nonce[CHACHA20_NONCE_SIZE],
			uint32_t counter,
			uint8_t *data, size_t len);

#endif
