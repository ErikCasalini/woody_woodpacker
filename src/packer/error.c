#include "error.h"
#include <stdio.h>

void	print_error(error_t error)
{
	switch (error)
	{
		case ok:
			break;
		case too_short:
			fprintf(stderr, "Error: file is too short for its ELF headers or segment contents\n");
			break;
		case set_uid:
			fprintf(stderr, "Error: set-user-ID and set-group-ID files are not supported\n");
			break;
		case not_a_file:
			fprintf(stderr, "Error: target is not a regular file\n");
			break;
		case bad_ident:
			fprintf(stderr, "Error: invalid or unsupported ELF identification\n");
			break;
		case bad_type:
			fprintf(stderr, "Error: ELF type must be ET_EXEC or ET_DYN\n");
			break;
		case bad_machine:
			fprintf(stderr, "Error: only x86-64 ELF files are supported\n");
			break;
		case bad_version:
			fprintf(stderr, "Error: unsupported ELF version\n");
			break;
		case bad_flags:
			fprintf(stderr, "Error: invalid ELF flags\n");
			break;
		case bad_ehsize:
			fprintf(stderr, "Error: invalid ELF header size\n");
			break;
		case bad_phentsize:
			fprintf(stderr, "Error: invalid program header entry size\n");
			break;
		case bad_shoff:
			fprintf(stderr, "Error: invalid section header table offset\n");
			break;
		case no_stub_section:
			fprintf(stderr, "Error: .stub section not found\n");
			break;
		case no_phdr:
			fprintf(stderr, "Error: ELF program header table is missing\n");
			break;
		case no_ptload:
			fprintf(stderr, "Error: ELF has no PT_LOADs\n");
			break;
		case bad_phdr:
			fprintf(stderr, "Error: invalid ELF program header\n");
			break;
		case segment_overflow:
			fprintf(stderr, "Error: segment size or address overflows during payload layout\n");
			break;
		case bad_pt_load_order:
			fprintf(stderr, "Error: PT_LOAD segments are not ordered by virtual address\n");
			break;
		case bad_pt_load_align:
			fprintf(stderr, "Error: PT_LOAD virtual address and file offset have incompatible alignment\n");
			break;
		case bad_p_align:
			fprintf(stderr, "Error: invalid segment alignment\n");
			break;
		case bad_interp_name:
			fprintf(stderr, "Error: invalid ELF interpreter name\n");
			break;
		case bad_page_sz:
			fprintf(stderr, "Error: unable to determine system page size\n");
			break;
		case et_exec_collide:
			fprintf(stderr, "Error: no address range available for the stub without overlapping the target ELF\n");
			break;
		case write_error:
			fprintf(stderr, "Error: unable to write output file\n");
			break;
		case bad_open:
			perror("Error: open()");
			break;
		case bad_mmap:
			perror("Error: mmap()");
			break;
		case bad_munmap:
			perror("Error: munmap()");
			break;
		case bad_fstat:
			perror("Error: fstat()");
			break;
		case bad_close:
			perror("Error: close()");
			break;
		default:
			fprintf(stderr, "Error: unexpected error (%d)\n", error);
	}
}
