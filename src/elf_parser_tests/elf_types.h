#ifndef ELF_TYPES_H
# define ELF_TYPES_H

# include <stdint.h>

/* e_ident magic bytes: 0x7f, 'E', 'L', 'F' */
typedef enum e_elf_magic
{
	elf_magic = 0x464c457f
} t_elf_magic;

/* e_ident[EI_CLASS] */
typedef enum e_elf_class
{
	elf_class_none = 0x00,
	elf_class_32 = 0x01,
	elf_class_64 = 0x02
} t_elf_class;

/* e_ident[EI_DATA] */
typedef enum e_elf_data
{
	elf_data_none = 0x00,
	elf_data_little_endian = 0x01,
	elf_data_big_endian = 0x02
} t_elf_data;

/* e_ident[EI_VERSION] and e_version */
typedef enum e_elf_version
{
	elf_version_none = 0x00,
	elf_version_current = 0x01
} t_elf_version;

/* e_ident[EI_OSABI] */
typedef enum e_elf_os_abi
{
	elf_os_abi_system_v = 0x00,
	elf_os_abi_hp_ux = 0x01,
	elf_os_abi_netbsd = 0x02,
	elf_os_abi_gnu_linux = 0x03,
	elf_os_abi_solaris = 0x06,
	elf_os_abi_aix = 0x07,
	elf_os_abi_irix = 0x08,
	elf_os_abi_freebsd = 0x09,
	elf_os_abi_tru64 = 0x0a,
	elf_os_abi_modesto = 0x0b,
	elf_os_abi_openbsd = 0x0c,
	elf_os_abi_openvms = 0x0d,
	elf_os_abi_nsk = 0x0e,
	elf_os_abi_aros = 0x0f,
	elf_os_abi_fenixos = 0x10,
	elf_os_abi_cloudabi = 0x11,
	elf_os_abi_openvos = 0x12,
	elf_os_abi_arm = 0x40,
	elf_os_abi_standalone = 0xff
} t_elf_os_abi;

/* e_type */
typedef enum e_elf_type
{
	elf_type_none = 0x0000,
	elf_type_relocatable = 0x0001,
	elf_type_executable = 0x0002,
	elf_type_dynamic = 0x0003,
	elf_type_core = 0x0004,
	elf_type_os_specific_low = 0xfe00,
	elf_type_os_specific_high = 0xfeff,
	elf_type_processor_specific_low = 0xff00,
	elf_type_processor_specific_high = 0xffff
} t_elf_type;

/* e_machine (architectures commonly encountered or currently relevant) */
typedef enum e_elf_machine
{
	elf_machine_none = 0x0000,
	elf_machine_sparc = 0x0002,
	elf_machine_x86 = 0x0003,
	elf_machine_mips = 0x0008,
	elf_machine_powerpc = 0x0014,
	elf_machine_powerpc64 = 0x0015,
	elf_machine_s390 = 0x0016,
	elf_machine_arm = 0x0028,
	elf_machine_superh = 0x002a,
	elf_machine_ia_64 = 0x0032,
	elf_machine_x86_64 = 0x003e,
	elf_machine_aarch64 = 0x00b7,
	elf_machine_risc_v = 0x00f3,
	elf_machine_bpf = 0x00f7,
	elf_machine_loongarch = 0x0102
} t_elf_machine;

/* p_type */
typedef enum e_elf_program_type
{
	elf_program_type_null = 0x00000000,
	elf_program_type_load = 0x00000001,
	elf_program_type_dynamic = 0x00000002,
	elf_program_type_interp = 0x00000003,
	elf_program_type_note = 0x00000004,
	elf_program_type_shlib = 0x00000005,
	elf_program_type_phdr = 0x00000006,
	elf_program_type_tls = 0x00000007,
	elf_program_type_os_specific_low = 0x60000000,
	elf_program_type_gnu_eh_frame = 0x6474e550,
	elf_program_type_gnu_stack = 0x6474e551,
	elf_program_type_gnu_relro = 0x6474e552,
	elf_program_type_gnu_property = 0x6474e553,
	elf_program_type_os_specific_high = 0x6fffffff,
	elf_program_type_processor_specific_low = 0x70000000,
	elf_program_type_processor_specific_high = 0x7fffffff
} t_elf_program_type;

typedef enum e_elf_program_flags
{
	elf_program_flag_execute = 0x00000001,
	elf_program_flag_write = 0x00000002,
	elf_program_flag_read = 0x00000004,
	elf_program_flag_os_specific = 0x0ff00000,
	elf_program_flag_processor_specific = 0xf0000000
} t_elf_program_flags;

typedef enum e_elf_d_tag
{
	DT_NULL				= 0,
	DT_NEEDED			= 1,
	DT_PLTRELSZ			= 2,
	DT_PLTGOT			= 3,
	DT_HASH				= 4,
	DT_STRTAB			= 5,
	DT_SYMTAB			= 6,
	DT_RELA				= 7,
	DT_RELASZ			= 8,
	DT_RELAENT			= 9,
	DT_STRSZ			= 10,
	DT_SYMENT			= 11,
	DT_INIT				= 12,
	DT_FINI				= 13,
	DT_SONAME			= 14,
	DT_RPATH			= 15,
	DT_SYMBOLIC			= 16,
	DT_REL				= 17,
	DT_RELSZ			= 18,
	DT_RELENT			= 19,
	DT_PLTREL			= 20,
	DT_DEBUG			= 21,
	DT_TEXTREL			= 22,
	DT_JMPREL			= 23,
	DT_BIND_NOW			= 24,
	DT_INIT_ARRAY		= 25,
	DT_FINI_ARRAY		= 26,
	DT_INIT_ARRAYSZ		= 27,
	DT_FINI_ARRAYSZ		= 28,
	DT_RUNPATH			= 29,
	DT_FLAGS			= 30,

	DT_PREINIT_ARRAY	= 32,
	DT_PREINIT_ARRAYSZ	= 33,
	DT_SYMTAB_SHNDX		= 34,
	DT_RELRSZ			= 35,
	DT_RELR				= 36,
	DT_RELRENT			= 37,
	DT_SYMTABSZ			= 39,

	DT_LOOS				= 0x6000000D,
	DT_HIOS				= 0x6FFFF000,
	DT_LOPROC			= 0x70000000,
	DT_HIPROC			= 0x7FFFFFFF
}	t_elf_d_tag;

typedef enum e_rel_type
{
	R_X86_64_NONE = 0,
	R_X86_64_64 = 1,
	R_X86_64_PC32 = 2,
	R_X86_64_GOT32 = 3,
	R_X86_64_PLT32 = 4,
	R_X86_64_COPY = 5,
	R_X86_64_GLOB_DAT = 6,
	R_X86_64_JUMP_SLOT = 7,
	R_X86_64_RELATIVE = 8,
	R_X86_64_GOTPCREL = 9,
	R_X86_64_32 = 10,
	R_X86_64_32S = 11,
	R_X86_64_16 = 12,
	R_X86_64_PC16 = 13,
	R_X86_64_8 = 14,
	R_X86_64_PC8 = 15,
	R_X86_64_DTPMOD64 = 16,
	R_X86_64_DTPOFF64 = 17,
	R_X86_64_TPOFF64 = 18,
	R_X86_64_TLSGD = 19,
	R_X86_64_TLSLD = 20,
	R_X86_64_DTPOFF32 = 21,
	R_X86_64_GOTTPOFF = 22,
	R_X86_64_TPOFF32 = 23,
	R_X86_64_PC64 = 24,
	R_X86_64_GOTOFF64 = 25,
	R_X86_64_GOTPC32 = 26,
	R_X86_64_SIZE32 = 32,
	R_X86_64_SIZE64 = 33,
	R_X86_64_GOTPC32_TLSDESC = 34,
	R_X86_64_TLSDESC_CALL = 35,
	R_X86_64_TLSDESC = 36,
	R_X86_64_IRELATIVE = 37,
	R_X86_64_RELATIVE64 = 38,
	R_X86_64_GOTPCRELX = 41,
	R_X86_64_REX_GOTPCRELX = 42,
	R_X86_64_CODE_4_GOTPCRELX = 43,
	R_X86_64_CODE_4_GOTTPOFF = 44,
	R_X86_64_CODE_4_GOTPC32_TLSDESC = 45,
	R_X86_64_CODE_5_GOTPCRELX = 46,
	R_X86_64_CODE_5_GOTTPOFF = 47,
	R_X86_64_CODE_5_GOTPC32_TLSDESC = 48,
	R_X86_64_CODE_6_GOTPCRELX = 49,
	R_X86_64_CODE_6_GOTTPOFF = 50,
	R_X86_64_CODE_6_GOTPC32_TLSDESC = 51
} t_rel_type;

typedef uint64_t t_ELF64_relr;

typedef struct s_ELF64_rela
{
	uint64_t	r_offset;
	uint64_t	r_info;
	uint64_t	r_addend;
} t_ELF64_rela;

typedef struct s_ELF64_rel
{
	uint64_t	r_offset;
	uint64_t	r_info;
} t_ELF64_rel;

typedef struct s_ELF64_pt_dynamic
{
	uint64_t d_tag;
	uint64_t d_value;
} t_ELF64_pt_dynamic;

typedef struct	s_ELF64_program_header
{
	uint32_t	type;
	uint32_t	flags;
	uint64_t	offset;
	uint64_t	vaddr;
	uint64_t	paddr;
	uint64_t	filesz;
	uint64_t	memsz;
	uint64_t	align;
} t_ELF64_program_header;

typedef struct s_ident
{
	uint8_t	magic[4];
	uint8_t	elf_class;
	uint8_t	data;
	uint8_t	version;
	uint8_t	os_abi;
	uint8_t	abi_version;
	uint8_t	padding[7];
} t_ident;

typedef struct	s_ELF64_header
{
	t_ident		ident;
	uint16_t	type;
	uint16_t	machine;
	uint32_t	version;
	uint64_t	entry;
	uint64_t	program_header_offset;
	uint64_t	section_header_offset;
	uint32_t	flags;
	uint16_t	header_size;
	uint16_t	program_header_entry_size;
	uint16_t	program_header_number;
	uint16_t	section_header_entry_size;
	uint16_t	section_header_number;
	uint16_t	section_header_string_index;
} t_ELF64_header;

typedef struct	s_ELF64
{
	char					*filename;
	t_ELF64_header			header;
	t_ELF64_program_header	*program_header;
} t_ELF64;

#endif
