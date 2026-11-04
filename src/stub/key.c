#include "key.h"
#include "../common/chacha20.h"
#include "../ft/libft.h"

static int char_to_hex(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return (c - '0');
    if (c >= 'a' && c <= 'f')
        return (c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
        return (c - 'A' + 10);
    return (-1);
}

stub_error_t	convert_arg_to_key(const char *arg, uint8_t *key)
{
	size_t	arg_len;
	size_t	key_sz;
	int		tmp;
	uint8_t	val;

	if (!arg || !key)
		return (bad_key);

	/* Each byte is represented by 2 hex characters.
	We also take in account "0x" before the key contents. */
	arg_len = ft_strlen(arg);
	key_sz = CHACHA20_KEY_SIZE * 2;

	if (!key_sz || arg_len != key_sz + 2)
		return (bad_key);

	if (arg[0] != '0' || (arg[1] != 'x' && arg[1] != 'X'))
		return (bad_key);
	arg += 2;

	for (size_t i = 0; i < CHACHA20_KEY_SIZE; i ++) {
		tmp = char_to_hex(arg[2 * i]);
		if (tmp < 0)
			return (bad_key);
		val = tmp * 0x10;

		tmp = char_to_hex(arg[2 * i + 1]);
		if (tmp < 0)
			return (bad_key);
		val += tmp;

		key[i] = val;
	}

	return (ok);
}
