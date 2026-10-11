.intel_syntax noprefix

.text

.global sys_lseek
.type sys_lseek, @function
sys_lseek:
	mov eax, 0x8
	syscall
	ret

.size sys_lseek, .-sys_lseek
