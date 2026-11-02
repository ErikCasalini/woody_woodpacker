#include "payload.h"
#include <stddef.h>
#include "../common/chacha20.h"
#include <elf.h>

/* The stub assumes that the original ELF is encrypted/compressed
 in the last PT_LOAD. At its first offset, there must be a
 payload header containing its metadata. */
payload_header_t	*find_elf_payload_header(const stub_info_t *stub)
{
	static const unsigned char	magic[] = PAYLOAD_MAGIC;
	payload_header_t	*payload_header;
	Elf64_Half	phnum;

	if (!stub || !stub->phdr)
		return (NULL);

	phnum = stub->header->e_phnum;
	while (phnum > 0 && stub->phdr[phnum - 1].p_type != PT_LOAD)
		phnum--;
	if (phnum == 0)
		return (NULL);
	payload_header = (payload_header_t*)((unsigned char*)stub->phdr[phnum - 1].p_vaddr + stub->load_bias);

	for (size_t i = 0; i < sizeof(magic); i++) {
		if (payload_header->magic[i] != magic[i])
			return (NULL);
	}

	return (payload_header);
}

Elf64_Ehdr	*decrypt_payload(payload_header_t *payload_header)
{
  static const uint8_t  nonce[CHACHA20_NONCE_SIZE] = {0};
  Elf64_Ehdr  *elf;

  elf = (Elf64_Ehdr*)((unsigned char*)payload_header + payload_header->offset);

  chacha20_crypt(payload_header->key, nonce, 0,
    (uint8_t*)elf, payload_header->decompressed_size);

  if ((elf->e_ident[EI_MAG0] != ELFMAG0)
    || (elf->e_ident[EI_MAG1] != ELFMAG1)
    || (elf->e_ident[EI_MAG2] != ELFMAG2)
    || (elf->e_ident[EI_MAG3] != ELFMAG3))
    return (NULL);

	return (elf);
}
