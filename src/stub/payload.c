#include <stddef.h>
#include <sys/mman.h>
#include <elf.h>
#include "payload.h"
#include "../common/chacha20.h"
#include "../ft/libft.h"
#include "messages.h"
#include "syscalls/syscalls.h"
#include "elf_info.h"
#include "../common/lzss.h"

/* The stub assumes that the original ELF is encrypted/compressed
 in the last PT_LOAD. At its first offset, there must be a
 payload header containing its metadata. */
stub_error_t	extract_payload(packed_payload_t *payload, const stub_info_t *stub)
{
	static const unsigned char	magic[] = PAYLOAD_MAGIC;
	payload_header_t			*payload_header;
	Elf64_Phdr					*segment;
	Elf64_Half					phnum;

	if (!payload || !stub || !stub->phdr)
		return (unexpected);

	phnum = stub->header->e_phnum;
	while (phnum > 0 && stub->phdr[phnum - 1].p_type != PT_LOAD)
		phnum--;
	if (phnum == 0)
		return (payload_not_found);
	segment = &stub->phdr[phnum - 1];

	/* Even if packer must give us a well formatted binary
	some quick checks don't hurt */
	if (segment->p_filesz < sizeof(payload_header_t))
		return (corrupted);

	payload_header = (payload_header_t *)((unsigned char *)segment->p_vaddr + stub->load_bias);

	/* check magic */
	for (size_t i = 0; i < sizeof(magic); i++) {
		if (payload_header->magic[i] != magic[i])
			return (payload_not_found);
	}

	if (payload_header->offset < sizeof(payload_header_t)
    || payload_header->offset > segment->p_filesz)
    	return (corrupted);

	if (payload_header->compressed_size > segment->p_filesz - payload_header->offset)
    	return (corrupted);

	payload->header.decompressed_size = payload_header->decompressed_size;
	payload->header.compressed_size = payload_header->compressed_size;
	payload->header.offset = payload_header->offset;
	payload->data = (uint8_t*)payload_header + payload_header->offset;

	return (ok);
}

stub_error_t unpack_payload(packed_payload_t *payload, elf_info_t *elf)
{
	intptr_t	map_addr;
	size_t		size;

  	/* Each packed ELF uses a unique key, so a fixed nonce is sufficient */
	chacha20_crypt(payload->key, payload->nonce, 0, payload->data,
  				payload->header.compressed_size);

	map_addr = (intptr_t)sys_mmap(NULL, payload->header.decompressed_size,
						PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS,
						-1, 0);
	if (map_addr < 0)
		return (bad_mmap);

	size = lzss_decompress(payload->data, payload->header.compressed_size,
					(uint8_t*)map_addr, payload->header.decompressed_size);
	if (size == LZSS_ERROR || size != payload->header.decompressed_size) {
		sys_munmap((uint8_t*)map_addr, payload->header.decompressed_size);
		return (bad_decompr);
	}

	elf->file_header = (Elf64_Ehdr*)map_addr;

	if (elf->file_header->e_ident[EI_MAG0] != ELFMAG0
  	|| elf->file_header->e_ident[EI_MAG1] != ELFMAG1
  	|| elf->file_header->e_ident[EI_MAG2] != ELFMAG2
  	|| elf->file_header->e_ident[EI_MAG3] != ELFMAG3) {
		elf->file_header = NULL;
		sys_munmap((uint8_t*)map_addr, payload->header.decompressed_size);
		return (corrupted);
	  }

	elf->file_sz = payload->header.decompressed_size;
	elf->file_phdr = (Elf64_Phdr *)((unsigned char*)elf->file_header +
					elf->file_header->e_phoff);

	return (ok);
}
