.intel_syntax noprefix

.text

.global jump_to_with_stack

jump_to_with_stack:
	mov	rsp, rsi
	xor rdx, rdx ; <-- glibc uses this to call a dl function, if the elf is static this must be NULL, else the dl will update this value on his own
	jmp	rdi


.size jump_to_with_stack, .-jump_to_with_stack
.type jump_to_with_stack, @function

.section .note.GNU-stack, "", @progbits
