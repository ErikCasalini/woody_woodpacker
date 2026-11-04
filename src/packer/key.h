#ifndef PACKER_KEY_H
# define PACKER_KEY_H

# include <stdint.h>
# include "error.h"

error_t	generate_key(uint8_t *key);
void	print_key(const uint8_t *key);

#endif
