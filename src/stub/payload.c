#include "payload.h"
#include <stddef.h>

/* The stub assumes that the original ELF is encrypted/compressed
 in the last PT_LOAD. At its first offset, there must be a
 payload header containing its metadata. */
payload_header_t	*find_elf_payload_header(const elf_info_t *this)
{
	static const unsigned char	magic[] = PAYLOAD_MAGIC;
	payload_header_t	*payload_header;
	Elf64_Half	phnum;

	if (!this || !this->mem_phdr)
		return (NULL);

	phnum = this->mem_header->e_phnum;
	while (phnum > 0 && this->mem_phdr[phnum - 1].p_type != PT_LOAD)
		phnum--;
	if (phnum == 0)
		return (NULL);
	payload_header = (payload_header_t*)((unsigned char*)this->mem_phdr[phnum - 1].p_vaddr + this->load_bias);

	for (size_t i = 0; i < sizeof(magic); i++) {
		if (payload_header->magic[i] != magic[i])
			return (NULL);
	}

	return (payload_header);
}

/* No encryption for now */
Elf64_Ehdr	*decrypt_payload(payload_header_t *payload_header)
{
	return ((Elf64_Ehdr*)((unsigned char*)payload_header + payload_header->offset));
}
