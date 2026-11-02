#include "payload.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <elf.h>
#include "../common/chacha20.h"
#include "../common/lzss.h"
#include "../common/page.h"
#include "syscalls/syscalls.h"

/* The stub assumes that the original ELF is encrypted/compressed
 in the last PT_LOAD. At its first offset, there must be a
 payload header containing its metadata. */
payload_header_t    *find_elf_payload_header(const stub_info_t *stub)
{
    static const unsigned char  magic[] = PAYLOAD_MAGIC;
    payload_header_t    *payload_header;
    Elf64_Half  phnum;

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

static int  is_valid_elf(const Elf64_Ehdr *elf)
{
    return (elf->e_ident[EI_MAG0] == ELFMAG0
        && elf->e_ident[EI_MAG1] == ELFMAG1
        && elf->e_ident[EI_MAG2] == ELFMAG2
        && elf->e_ident[EI_MAG3] == ELFMAG3);
}

uint8_t *decrypt_payload(payload_header_t *payload_header)
{
    static const uint8_t  nonce[CHACHA20_NONCE_SIZE] = {0};
    uint8_t               *data;

    data = (uint8_t*)payload_header + payload_header->offset;
    chacha20_crypt(payload_header->key, nonce, 0,
        data, payload_header->compressed_size);
    return (data);
}

Elf64_Ehdr *decompress_payload(const payload_header_t *payload_header,
            uint8_t *compressed, size_t page_size)
{
    Elf64_Ehdr  *elf;
    void        *region;
    size_t      map_len;
    size_t      produced;

    if (payload_header->compressed_size == payload_header->decompressed_size) {
        elf = (Elf64_Ehdr*)compressed;
        if (!is_valid_elf(elf))
            return (NULL);
        return (elf);
    }

    map_len = round_to_page_up(payload_header->decompressed_size, page_size);
    region = sys_mmap(NULL, map_len, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((intptr_t)region < 0)
        return (NULL);

    produced = lzss_decompress(compressed, payload_header->compressed_size,
            (uint8_t*)region, payload_header->decompressed_size);
    if (produced != payload_header->decompressed_size) {
        sys_munmap(region, map_len);
        return (NULL);
    }

    elf = (Elf64_Ehdr*)region;
    if (!is_valid_elf(elf)) {
        sys_munmap(region, map_len);
        return (NULL);
    }
    return (elf);
}
