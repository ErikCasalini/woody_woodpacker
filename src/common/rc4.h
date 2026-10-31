#ifndef RC4_H
# define RC4_H

# include <stdlib.h>

void	rc4_crypt(const unsigned char *key, size_t keylen,
			unsigned char *data, size_t datalen);

#endif
