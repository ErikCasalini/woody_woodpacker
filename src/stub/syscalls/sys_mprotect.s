.intel_syntax noprefix

.text

.global sys_mprotect
.type sys_mprotect, @function
sys_mprotect:
	mov eax, 0xA
	syscall
	ret

.size sys_mprotect, .-sys_mprotect
