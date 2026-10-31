#include "rc4.h"

void	rc4_crypt(const unsigned char *key, size_t keylen,
			unsigned char *data, size_t datalen)
{
	unsigned char s[256];
	unsigned char tmp;
	int           i;
	int           j;

	if (keylen == 0)
		return ;

	i = -1;
	while (++i < 256)
		s[i] = (unsigned char)i;

	i = -1;
	j = 0;
	while (++i < 256) {
		j = (j + s[i] + key[i % keylen]) & 0xff;
		tmp = s[i];
		s[i] = s[j];
		s[j] = tmp;
	}
	i = 0;
	j = 0;
	for (size_t n = 0; n < datalen; n++) {
		i = (i + 1) & 0xff;
		j = (j + s[i]) & 0xff;
		tmp = s[i];
		s[i] = s[j];
		s[j] = tmp;
		data[n] ^= s[(s[i] + s[j]) & 0xff];
	}
}
