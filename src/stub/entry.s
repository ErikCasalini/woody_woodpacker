.intel_syntax noprefix


.section .text.stub_entry, "ax", @progbits

.global stub_entry
.type stub_entry, @function
stub_entry:
	mov	rdi, rsp
	call execute_packed_elf
	ud2

.size stub_entry, .-stub_entry
