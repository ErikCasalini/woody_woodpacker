#include "segment_mapper.h"
#include <unistd.h>
#include <sys/mman.h>
#include "elf_types.h"
#include "errors.h"


#include <string.h>

uint64_t	round_to_page_up(uint64_t value)
{
	uint32_t tmp;
	uint32_t page_size;

	page_size =  sysconf(_SC_PAGESIZE);
	tmp = value % page_size;
	if (tmp == 0)
		return (value);

	return (value + page_size - tmp);
}

uint64_t	round_to_page_down(uint64_t value)
{
	uint32_t tmp;
	uint32_t page_size;

	page_size =  sysconf(_SC_PAGESIZE);
	tmp = value % page_size;
	return (value - tmp);
}

uint32_t	elf_flags_to_prot(uint32_t flags)
{
    int prot;

    prot = 0;
    if (flags & elf_program_flag_read)
        prot |= PROT_READ;
    if (flags & elf_program_flag_write)
        prot |= PROT_WRITE;
    if (flags & elf_program_flag_execute)
        prot |= PROT_EXEC;
    return (prot);
}

void	*map_segment(int file, uint64_t addr, t_ELF64_program_header segment)
{ // ASKIP IL Y A UN PB AVEC LE MAPPING DU BSS
	uint64_t 	map_start;
	uint64_t 	map_end;
	uint64_t 	map_size;
	uint64_t	bss_size;
	uint64_t	end_of_mapped_file_data;
	uint64_t	remaining_space_in_mem_page;

	map_start = round_to_page_down(addr);
	map_end = round_to_page_up(addr + segment.filesz);
	map_size = map_end - map_start;
	bss_size = segment.memsz - segment.filesz;
	end_of_mapped_file_data = addr + segment.filesz;
	remaining_space_in_mem_page = map_end - end_of_mapped_file_data;
	// IL FAUDRAIT MAPPER D'ABORD TOUTE LA ZONE REQUISE PAR L'ELF (AVEC NO_REPLACE) POUR ETRE SURS QUE RIEN NE SERA ECRASE, PUIS DE REMPLIR A L'INTERIEUR (SANS NO_REPLACE) POUR PAS QU'IL Y AIT D'INCOMPATIBLITEES DANS LE CAS OU CERTAINS SEGMENT SE CHAUVAUCHENT
	void *mapped_addr = mmap((void*)map_start,
				map_size,
				elf_flags_to_prot(segment.flags) | PROT_WRITE,
				MAP_PRIVATE | MAP_FIXED_NOREPLACE,
				file,
				round_to_page_down(segment.offset));

	#include "stdio.h"
	printf("allocated: %p\n", mapped_addr);

	if (mapped_addr == MAP_FAILED)
		return (MAP_FAILED);

	if (bss_size)
		memset((void*)end_of_mapped_file_data, 0, remaining_space_in_mem_page);
	else
		return (mapped_addr);

	if (remaining_space_in_mem_page >= bss_size)
		return (mapped_addr);

	bss_size -= remaining_space_in_mem_page;
	mapped_addr = mmap((void*)(map_start + map_size),
		round_to_page_up(bss_size),
		elf_flags_to_prot(segment.flags) | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
		-1,
		0);

	printf("allocated: %p\n", mapped_addr);

	return (mapped_addr);
}

t_error	map_segments(uint64_t base_addr, int file, t_ELF64_program_header *program_header, uint32_t segment_num)
{
	for (uint32_t i = 0; i < segment_num; i++) {
		if (program_header[i].type != elf_program_type_load)
			continue;

		if (map_segment(file, base_addr + program_header[i].vaddr, program_header[i]) == MAP_FAILED)
			return (bad_mmap);
	}

	return (ok);
}
