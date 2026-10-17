.intel_syntax noprefix

.text

.global jump_to_with_stack
.type jump_to_with_stack, @function
jump_to_with_stack:
	mov	rsp, rsi
	xor rdx, rdx # glibc uses this to call a dl function; it must be NULL for a static ELF
	jmp	rdi

.size jump_to_with_stack, .-jump_to_with_stack

.section .note.GNU-stack, "", @progbits
