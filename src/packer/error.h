#ifndef PACKER_ERROR_H
# define PACKER_ERROR_H

typedef enum error_e
{
	ok,
	too_short,
	set_uid,
	not_a_file,
	bad_ident,
	bad_type,
	bad_machine,
	bad_version,
	bad_flags,
	bad_ehsize,
	bad_phentsize,
	bad_shoff,
	no_stub_section,
	no_phdr,
	no_ptload,
	bad_phdr,
	bad_entry,
	segment_overflow,
	bad_pt_load_order,
	bad_pt_load_align,
	bad_p_align,
	bad_interp_name,
	bad_page_sz,
	et_exec_collide,
	write_error,
	unexpected_error,
	bad_open,
	bad_mmap,
	bad_munmap,
	bad_fstat,
	bad_close,
	bad_read
} error_t;

void	print_error(error_t error);

#endif
