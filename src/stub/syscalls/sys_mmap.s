.intel_syntax noprefix

.text

.global sys_mmap
.type sys_mmap, @function
sys_mmap:
	mov eax, 0x9
	mov	r10, rcx
	syscall
	ret

.size sys_mmap, .-sys_mmap
