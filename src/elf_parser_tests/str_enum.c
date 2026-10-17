#include "str_enum.h"

const char	*str_class(t_elf_class elf_class)
{
	switch (elf_class)
	{
		case elf_class_none:
			return ("None");
		case elf_class_32:
			return ("ELF32");
		case elf_class_64:
			return ("ELF64");
		default:
			return ("Unknown ELF class");
	}
}

const char	*str_data(t_elf_data data)
{
	switch (data)
	{
		case elf_data_none:
			return ("None");
		case elf_data_little_endian:
			return ("Little endian");
		case elf_data_big_endian:
			return ("Big endian");
		default:
			return ("Unknown ELF data encoding");
	}
}

const char	*str_version(t_elf_version version)
{
	switch (version)
	{
		case elf_version_none:
			return ("None");
		case elf_version_current:
			return ("Current");
		default:
			return ("Unknown ELF version");
	}
}

const char	*str_os_abi(t_elf_os_abi os_abi)
{
	switch (os_abi)
	{
		case elf_os_abi_system_v:
			return ("System V");
		case elf_os_abi_hp_ux:
			return ("HP-UX");
		case elf_os_abi_netbsd:
			return ("NetBSD");
		case elf_os_abi_gnu_linux:
			return ("GNU/Linux");
		case elf_os_abi_solaris:
			return ("Solaris");
		case elf_os_abi_aix:
			return ("AIX");
		case elf_os_abi_irix:
			return ("IRIX");
		case elf_os_abi_freebsd:
			return ("FreeBSD");
		case elf_os_abi_tru64:
			return ("Tru64");
		case elf_os_abi_modesto:
			return ("Modesto");
		case elf_os_abi_openbsd:
			return ("OpenBSD");
		case elf_os_abi_openvms:
			return ("OpenVMS");
		case elf_os_abi_nsk:
			return ("NSK");
		case elf_os_abi_aros:
			return ("AROS");
		case elf_os_abi_fenixos:
			return ("FenixOS");
		case elf_os_abi_cloudabi:
			return ("CloudABI");
		case elf_os_abi_openvos:
			return ("OpenVOS");
		case elf_os_abi_arm:
			return ("ARM");
		case elf_os_abi_standalone:
			return ("Standalone");
		default:
			return ("Unknown ELF OS ABI");
	}
}

const char	*str_type(t_elf_type type)
{
	switch (type)
	{
		case elf_type_none:
			return ("None");
		case elf_type_relocatable:
			return ("Relocatable");
		case elf_type_executable:
			return ("Executable");
		case elf_type_dynamic:
			return ("Dynamic");
		case elf_type_core:
			return ("Core");
		case elf_type_os_specific_low:
			return ("OS-specific low");
		case elf_type_os_specific_high:
			return ("OS-specific high");
		case elf_type_processor_specific_low:
			return ("Processor-specific low");
		case elf_type_processor_specific_high:
			return ("Processor-specific high");
		default:
			return ("Unknown ELF type");
	}
}

const char	*str_machine(t_elf_machine machine)
{
	switch (machine)
	{
		case elf_machine_none:
			return ("None");
		case elf_machine_sparc:
			return ("SPARC");
		case elf_machine_x86:
			return ("x86");
		case elf_machine_mips:
			return ("MIPS");
		case elf_machine_powerpc:
			return ("PowerPC");
		case elf_machine_powerpc64:
			return ("PowerPC64");
		case elf_machine_s390:
			return ("S390");
		case elf_machine_arm:
			return ("ARM");
		case elf_machine_superh:
			return ("SuperH");
		case elf_machine_ia_64:
			return ("IA-64");
		case elf_machine_x86_64:
			return ("x86-64");
		case elf_machine_aarch64:
			return ("AArch64");
		case elf_machine_risc_v:
			return ("RISC-V");
		case elf_machine_bpf:
			return ("BPF");
		case elf_machine_loongarch:
			return ("LoongArch");
		default:
			return ("Unknown ELF machine");
	}
}

const char	*str_program_type(t_elf_program_type type)
{
	switch (type)
	{
		case elf_program_type_null:
			return ("PT_NULL");
		case elf_program_type_load:
			return ("PT_LOAD");
		case elf_program_type_dynamic:
			return ("PT_DYNAMIC");
		case elf_program_type_interp:
			return ("PT_INTERP");
		case elf_program_type_note:
			return ("PT_NOTE");
		case elf_program_type_shlib:
			return ("PT_SHLIB");
		case elf_program_type_phdr:
			return ("PT_PHDR");
		case elf_program_type_tls:
			return ("PT_TLS");
		case elf_program_type_os_specific_low:
			return ("PT_LOOS");
		case elf_program_type_gnu_eh_frame:
			return ("PT_GNU_EH_FRAME");
		case elf_program_type_gnu_stack:
			return ("PT_GNU_STACK");
		case elf_program_type_gnu_relro:
			return ("PT_GNU_RELRO");
		case elf_program_type_gnu_property:
			return ("PT_GNU_PROPERTY");
		case elf_program_type_os_specific_high:
			return ("PT_HIOS");
		case elf_program_type_processor_specific_low:
			return ("PT_LOPROC");
		case elf_program_type_processor_specific_high:
			return ("PT_HIPROC");
		default:
			return ("PT_UNKNOWN");
	}
}

const char	*str_program_flags(t_elf_program_flags flags)
{
	static const char	*permissions[8] = {
		"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"
	};

	return (permissions[(unsigned int)flags & 0x7]); //exclude special flags
}

const char	*str_d_tag(t_elf_d_tag d_tag)
{
	switch (d_tag)
	{
		case DT_NULL:
			return ("DT_NULL");
		case DT_NEEDED:
			return ("DT_NEEDED");
		case DT_PLTRELSZ:
			return ("DT_PLTRELSZ");
		case DT_PLTGOT:
			return ("DT_PLTGOT");
		case DT_HASH:
			return ("DT_HASH");
		case DT_STRTAB:
			return ("DT_STRTAB");
		case DT_SYMTAB:
			return ("DT_SYMTAB");
		case DT_RELA:
			return ("DT_RELA");
		case DT_RELASZ:
			return ("DT_RELASZ");
		case DT_RELAENT:
			return ("DT_RELAENT");
		case DT_STRSZ:
			return ("DT_STRSZ");
		case DT_SYMENT:
			return ("DT_SYMENT");
		case DT_INIT:
			return ("DT_INIT");
		case DT_FINI:
			return ("DT_FINI");
		case DT_SONAME:
			return ("DT_SONAME");
		case DT_RPATH:
			return ("DT_RPATH");
		case DT_SYMBOLIC:
			return ("DT_SYMBOLIC");
		case DT_REL:
			return ("DT_REL");
		case DT_RELSZ:
			return ("DT_RELSZ");
		case DT_RELENT:
			return ("DT_RELENT");
		case DT_PLTREL:
			return ("DT_PLTREL");
		case DT_DEBUG:
			return ("DT_DEBUG");
		case DT_TEXTREL:
			return ("DT_TEXTREL");
		case DT_JMPREL:
			return ("DT_JMPREL");
		case DT_BIND_NOW:
			return ("DT_BIND_NOW");
		case DT_INIT_ARRAY:
			return ("DT_INIT_ARRAY");
		case DT_FINI_ARRAY:
			return ("DT_FINI_ARRAY");
		case DT_INIT_ARRAYSZ:
			return ("DT_INIT_ARRAYSZ");
		case DT_FINI_ARRAYSZ:
			return ("DT_FINI_ARRAYSZ");
		case DT_RUNPATH:
			return ("DT_RUNPATH");
		case DT_FLAGS:
			return ("DT_FLAGS");
		case DT_PREINIT_ARRAY:
			return ("DT_PREINIT_ARRAY");
		case DT_PREINIT_ARRAYSZ:
			return ("DT_PREINIT_ARRAYSZ");
		case DT_SYMTAB_SHNDX:
			return ("DT_SYMTAB_SHNDX");
		case DT_RELRSZ:
			return ("DT_RELRSZ");
		case DT_RELR:
			return ("DT_RELR");
		case DT_RELRENT:
			return ("DT_RELRENT");
		case DT_SYMTABSZ:
			return ("DT_SYMTABSZ");
		case DT_LOOS:
			return ("DT_LOOS");
		case DT_HIOS:
			return ("DT_HIOS");
		case DT_LOPROC:
			return ("DT_LOPROC");
		case DT_HIPROC:
			return ("DT_HIPROC");
		default:
			return ("DT_UNKNOWN");
	}
}

const char	*str_rel_type(t_rel_type rel_type)
{
	switch (rel_type)
	{
		case R_X86_64_NONE:
			return ("R_X86_64_NONE");
		case R_X86_64_64:
			return ("R_X86_64_64");
		case R_X86_64_PC32:
			return ("R_X86_64_PC32");
		case R_X86_64_GOT32:
			return ("R_X86_64_GOT32");
		case R_X86_64_PLT32:
			return ("R_X86_64_PLT32");
		case R_X86_64_COPY:
			return ("R_X86_64_COPY");
		case R_X86_64_GLOB_DAT:
			return ("R_X86_64_GLOB_DAT");
		case R_X86_64_JUMP_SLOT:
			return ("R_X86_64_JUMP_SLOT");
		case R_X86_64_RELATIVE:
			return ("R_X86_64_RELATIVE");
		case R_X86_64_GOTPCREL:
			return ("R_X86_64_GOTPCREL");
		case R_X86_64_32:
			return ("R_X86_64_32");
		case R_X86_64_32S:
			return ("R_X86_64_32S");
		case R_X86_64_16:
			return ("R_X86_64_16");
		case R_X86_64_PC16:
			return ("R_X86_64_PC16");
		case R_X86_64_8:
			return ("R_X86_64_8");
		case R_X86_64_PC8:
			return ("R_X86_64_PC8");
		case R_X86_64_DTPMOD64:
			return ("R_X86_64_DTPMOD64");
		case R_X86_64_DTPOFF64:
			return ("R_X86_64_DTPOFF64");
		case R_X86_64_TPOFF64:
			return ("R_X86_64_TPOFF64");
		case R_X86_64_TLSGD:
			return ("R_X86_64_TLSGD");
		case R_X86_64_TLSLD:
			return ("R_X86_64_TLSLD");
		case R_X86_64_DTPOFF32:
			return ("R_X86_64_DTPOFF32");
		case R_X86_64_GOTTPOFF:
			return ("R_X86_64_GOTTPOFF");
		case R_X86_64_TPOFF32:
			return ("R_X86_64_TPOFF32");
		case R_X86_64_PC64:
			return ("R_X86_64_PC64");
		case R_X86_64_GOTOFF64:
			return ("R_X86_64_GOTOFF64");
		case R_X86_64_GOTPC32:
			return ("R_X86_64_GOTPC32");
		case R_X86_64_SIZE32:
			return ("R_X86_64_SIZE32");
		case R_X86_64_SIZE64:
			return ("R_X86_64_SIZE64");
		case R_X86_64_GOTPC32_TLSDESC:
			return ("R_X86_64_GOTPC32_TLSDESC");
		case R_X86_64_TLSDESC_CALL:
			return ("R_X86_64_TLSDESC_CALL");
		case R_X86_64_TLSDESC:
			return ("R_X86_64_TLSDESC");
		case R_X86_64_IRELATIVE:
			return ("R_X86_64_IRELATIVE");
		case R_X86_64_RELATIVE64:
			return ("R_X86_64_RELATIVE64");
		case R_X86_64_GOTPCRELX:
			return ("R_X86_64_GOTPCRELX");
		case R_X86_64_REX_GOTPCRELX:
			return ("R_X86_64_REX_GOTPCRELX");
		case R_X86_64_CODE_4_GOTPCRELX:
			return ("R_X86_64_CODE_4_GOTPCRELX");
		case R_X86_64_CODE_4_GOTTPOFF:
			return ("R_X86_64_CODE_4_GOTTPOFF");
		case R_X86_64_CODE_4_GOTPC32_TLSDESC:
			return ("R_X86_64_CODE_4_GOTPC32_TLSDESC");
		case R_X86_64_CODE_5_GOTPCRELX:
			return ("R_X86_64_CODE_5_GOTPCRELX");
		case R_X86_64_CODE_5_GOTTPOFF:
			return ("R_X86_64_CODE_5_GOTTPOFF");
		case R_X86_64_CODE_5_GOTPC32_TLSDESC:
			return ("R_X86_64_CODE_5_GOTPC32_TLSDESC");
		case R_X86_64_CODE_6_GOTPCRELX:
			return ("R_X86_64_CODE_6_GOTPCRELX");
		case R_X86_64_CODE_6_GOTTPOFF:
			return ("R_X86_64_CODE_6_GOTTPOFF");
		case R_X86_64_CODE_6_GOTPC32_TLSDESC:
			return ("R_X86_64_CODE_6_GOTPC32_TLSDESC");
		default:
			return ("R_X86_64_UNKNOWN");
	}
}
