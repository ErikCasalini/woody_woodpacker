#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include "debug.h"
#include "error.h"
#include "file.h"
#include "layout.h"
#include "output.h"
#include "stub_section.h"
#include "validate.h"

/*
   "packer": represents this program/file
   "target": represents the input ELF that will be packed
   "output": represents the final packed ELF
   "payload": represents the "target" ELF file embded inside of "output"
   "stub": represents the code/data that will run inside of "output"
    in order to find/decrypt/load/run "payload", and that is originally
	embded inside of "packer"
*/
int main(int argc, char *argv[])
{
	mapped_file_t 		target = {.data = MAP_FAILED};
	mapped_file_t 		packer = {.data = MAP_FAILED};
	int					output_fd = -1;
	payload_header_t	payload_header = {0};
	elf_layout_t		output_layout = {0};
	const Elf64_Shdr	*packer_stub_section;
	int64_t				page_size;
	error_t				error;
	int					status = 1;

	if (argc < 2)
		return (fprintf(stderr, "Specify a filename\n"), 1);

	error = map_file(argv[1], &target);
	if (error != ok) {
		print_error(error);
		goto cleanup;
	}

	page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0) {
		print_error(bad_page_sz);
		goto cleanup;
	}

	error = check_elf_validity(&target, page_size);
	if (error != ok) {
		print_error(error);
		goto cleanup;
	}

	error = map_file("/proc/self/exe", &packer);
	if (error != ok) {
		print_error(error);
		goto cleanup;
	}

	packer_stub_section = find_packer_section(&packer, ".stub");
	if (packer_stub_section == NULL) {
		print_error(no_stub_section);
		goto cleanup;
	}

	error = init_output_headers(
				&output_layout,
				&target,
				packer_stub_section,
				page_size
			);
	if (error != ok) {
		print_error(error);
		goto cleanup;
	}

	init_payload_header(&payload_header, &target);

	error = generate_payload_key(&payload_header);
	if (error != ok) {
		print_error(error);
		goto cleanup;
	}
	{
		const unsigned char	*key = payload_header.key;

		printf("key_value: 0x");
		for (size_t i = 0; i < sizeof(payload_header.key); i++)
			printf("%02x", key[i]);
		printf("\n");
	}

#ifdef DEBUG
	print_output_layout(&output_layout);
#endif

	output_fd = create_output_file(OUTPUT_FILE, &target.st);
	if (output_fd < 0) {
		perror("Error: create_output_file()");
		goto cleanup;
	}

	error = fill_output_elf(
				output_fd,
				&output_layout,
				packer_stub_section,
				packer.data,
				target.data,
				&payload_header
			);
	if (error != ok) {
		print_error(error);
		if (unlink(OUTPUT_FILE) < 0)
			perror("Error: unlink()");
		goto cleanup;
	}

	/* Success */
	status = 0;

cleanup:
	if (output_fd >= 0 && close(output_fd) < 0) {
		perror("Error: close()");
		status = 1;
	}
	if (target.data != MAP_FAILED && munmap(target.data, target.st.st_size) < 0)
		perror("Error: unmapping file");
	if (packer.data != MAP_FAILED && munmap(packer.data, packer.st.st_size) < 0)
		perror("Error: unmapping file");
	if (status == 0)
		printf("ELF successuflly packed!\n");

	return (status);
}
