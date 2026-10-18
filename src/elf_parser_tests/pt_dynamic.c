#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "elf_types.h"
#include "errors.h"
#include "pt_dynamic.h"
#include "str_enum.h"

uint64_t find_dynamic_tag(t_ELF64_pt_dynamic *_DYNAMIC, t_elf_d_tag tag)
{
	if (_DYNAMIC == NULL)
		return (printf("Invalid _DYNAMIC for search\n"), 0);
	if (tag == DT_NULL || tag == DT_NEEDED)
		return (printf("Invalid tag for search\n"), 0);

	for (int i = 0; _DYNAMIC[i].d_tag != DT_NULL; i++) {
		if (_DYNAMIC[i].d_tag == tag)
			return (_DYNAMIC[i].d_value);
	}
	return (0);
}

t_error	init_rel_struct(t_reloc_table_info *rel, t_ELF64_pt_dynamic *_DYNAMIC, t_elf_type elf_type, uint64_t base_addr)
{
	rel->addr = find_dynamic_tag(_DYNAMIC, DT_REL);
	if (!rel->addr)
		return (rel->status = not_present, ok);

	rel->size = find_dynamic_tag(_DYNAMIC, DT_RELSZ);
	rel->ent_size = find_dynamic_tag(_DYNAMIC, DT_RELENT);
	if (!rel->size || !rel->ent_size)
		return (rel->status = invalid, bad_dynamic);

	if (elf_type == elf_type_dynamic)
		rel->addr += base_addr;

	rel->status = valid;

	return (ok);
}

t_error	init_rela_struct(t_reloc_table_info *rela, t_ELF64_pt_dynamic *_DYNAMIC, t_elf_type elf_type, uint64_t base_addr)
{
	rela->addr = find_dynamic_tag(_DYNAMIC, DT_RELA);
	if (!rela->addr)
		return (rela->status = not_present, ok);

	rela->size = find_dynamic_tag(_DYNAMIC, DT_RELASZ);
	rela->ent_size = find_dynamic_tag(_DYNAMIC, DT_RELAENT);
	if (!rela->size || !rela->ent_size)
		return (rela->status = invalid, bad_dynamic);

	if (elf_type == elf_type_dynamic)
		rela->addr += base_addr;

	rela->status = valid;

	return (ok);
}

t_error	init_relr_struct(t_reloc_table_info *relr, t_ELF64_pt_dynamic *_DYNAMIC, t_elf_type elf_type, uint64_t base_addr)
{
	relr->addr = find_dynamic_tag(_DYNAMIC, DT_RELR);
	if (!relr->addr)
		return (relr->status = not_present, ok);

	relr->size = find_dynamic_tag(_DYNAMIC, DT_RELRSZ);
	relr->ent_size = find_dynamic_tag(_DYNAMIC, DT_RELRENT);
	if (!relr->size || !relr->ent_size)
		return (relr->status = invalid, bad_dynamic);

	if (elf_type == elf_type_dynamic)
			relr->addr += base_addr;

	relr->status = valid;

	return (ok);
}

void	print_rel_table(t_reloc_table_info table)
{
	t_ELF64_rel	*rel_table;
	int			table_size;

	printf("RELs:\n");

	table_size = table.size / table.ent_size;
	if (table_size <= 0) {
		printf("Empty\n");
		return ;
	}

	rel_table = (t_ELF64_rel *)table.addr;

	for (int i = 0; i < table_size; i++) {
		printf("--------------\n"
			   "Offset: %lx\n"
			   "Info Symbol: %lx\n"
			   "Info Type: %s\n"
			   "--------------\n",
				rel_table[i].r_offset,
				ELF64_R_SYM(rel_table[i].r_info),
				str_rel_type((t_rel_type)ELF64_R_TYPE(rel_table[i].r_info)));
	}
}

void	print_rela_table(t_reloc_table_info table)
{
	t_ELF64_rela	*rela_table;
	int				table_size;

	printf("RELAs:\n");

	table_size = table.size / table.ent_size;
	if (table_size <= 0) {
		printf("Empty\n");
		return ;
	}

	rela_table = (t_ELF64_rela *)table.addr;

	for (int i = 0; i < table_size; i++) {
		printf("--------------\n"
			   "Offset: %lx\n"
			   "Info Symbol: %lx\n"
			   "Info Type: %s\n"
			   "Addend: %lx\n"
			   "--------------\n",
				rela_table[i].r_offset,
				ELF64_R_SYM(rela_table[i].r_info),
				str_rel_type((t_rel_type)ELF64_R_TYPE(rela_table[i].r_info)),
				rela_table[i].r_addend);
	}
}

void	print_relr_table(t_reloc_table_info table)
{
	t_ELF64_relr	*relr_table;
	int				table_size;

	printf("RELRs:\n");

	relr_table = (t_ELF64_relr *)table.addr;

	table_size = table.size / table.ent_size;
	if (table_size <= 0) {
		printf("Empty\n");
		return ;
	}

	for (int i = 0; i < table_size; i++) {
		printf("%u: %lx\n", i, relr_table[i]);
	}

}

t_error	apply_relas(t_reloc_table_info table, t_elf_type elf_type, uint64_t base_addr)
{
	t_ELF64_rela	*rela_table;
	int				table_size;

	printf("Appling RELAs...\n");

	table_size = table.size / table.ent_size;

	rela_table = (t_ELF64_rela *)table.addr;

	for (int i = 0; i < table_size; i++) {
		if (ELF64_R_TYPE(rela_table[i].r_info) == R_X86_64_RELATIVE) {
			uint64_t *rela_addr;
			if (elf_type == elf_type_dynamic)
				rela_addr = (uint64_t*)(rela_table[i].r_offset + base_addr);
			else if (elf_type == elf_type_executable)
				rela_addr = (uint64_t*)rela_table[i].r_offset;
			else
				continue;
			*rela_addr = rela_table[i].r_addend + base_addr;
		}
	}
	printf("DONE\n");
	return (ok);
}

t_error print_relocations_fields(int file, t_ELF64 elf, uint64_t base_addr)
{
	t_ELF64_pt_dynamic *pt_dynamic = NULL;
	t_reloc_table_info	rel;
	t_reloc_table_info	rela;
	t_reloc_table_info	relr;
	ssize_t				ret;

	for (int i = 0; i < elf.header.program_header_number; i++) {
		if (elf.program_header[i].type == elf_program_type_dynamic) {
			if (lseek(file, elf.program_header[i].offset, SEEK_SET) < 0)
				return (bad_file);

			pt_dynamic = calloc(1, elf.program_header[i].filesz);
			if (pt_dynamic == NULL)
				return (bad_alloc);
			ret = read(file, pt_dynamic, elf.program_header[i].filesz);
			if (ret == -1 || (uint64_t)ret != elf.program_header[i].filesz)
				return (free(pt_dynamic), bad_file);
			break;
		}
	}
	if (pt_dynamic == NULL)
		return (printf("No relocations found\n"), ok);

	if (init_rel_struct(&rel, pt_dynamic, elf.header.type, base_addr) == bad_dynamic ||
		init_rela_struct(&rela, pt_dynamic, elf.header.type, base_addr) == bad_dynamic ||
		init_relr_struct(&relr, pt_dynamic, elf.header.type, base_addr) == bad_dynamic)
		return (free(pt_dynamic), bad_dynamic);

	printf("Relocations found inside PT_DYNAMIC :\n");
	if (rel.status == valid)
		print_rel_table(rel);
	if (rela.status == valid) {
		print_rela_table(rela);
		apply_relas(rela, elf.header.type, base_addr);
	}
	if (relr.status == valid)
		print_relr_table(relr);

	return (ok);
}

// void	parse_pt_dynamic(t_ELF64_pt_dynamic *_DYNAMIC)
// {
// 	for (int i = 0; _DYNAMIC[i].d_tag != DT_NULL; i++) {
// 		switch (_DYNAMIC[i].d_tag) {
// 			case DT_REL:
// 				print_rela();
// 			case DT_RELA

// 		}


// 	}
// }
