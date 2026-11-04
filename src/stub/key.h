#ifndef STUB_KEY_H
# define STUB_KEY_H

# include <stdint.h>
# include "messages.h"

stub_error_t	convert_arg_to_key(const char *arg, uint8_t *key);

#endif
