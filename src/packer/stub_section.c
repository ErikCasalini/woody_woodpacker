#include "stub_section.h"
#include "../ft/libft.h"

/* This function searches for a "name" section in a mapped file.
 The function must be used only on the tursted packer file produced
 by our build. For the moment, no bounds or overflow checks are performed.
 Malformed metadata may cause undefined behavior.
 If no "name" section is found, NULL is returned. */
const Elf64_Shdr	*find_packer_section(const mapped_file_t *packer, const char *name)
{
	Elf64_Ehdr	*header;
	Elf64_Shdr	*shdrs;
	char		*section_names;
	size_t		name_len;

	if (!packer || !name)
		return (NULL);

	name_len = ft_strlen(name);
	header = packer->data;
	if (!header
		|| !header->e_shoff
		|| name_len < 1)
		return (NULL);

	shdrs = (Elf64_Shdr*)((unsigned char*)header + header->e_shoff);
	section_names = (char*)header + shdrs[header->e_shstrndx].sh_offset;

	for (Elf64_Half i = 0; i < header->e_shnum; i++) {
		if (ft_strncmp(section_names + shdrs[i].sh_name,
					name,
					name_len + 1
				) == 0
			) {
			return (&shdrs[i]);
		}
	}

	return (NULL);
}
