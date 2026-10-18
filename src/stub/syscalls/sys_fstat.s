.intel_syntax noprefix

.text

.global sys_fstat
.type sys_fstat, @function
sys_fstat:
	mov eax, 0x5
	syscall
	ret

.size sys_fstat, .-sys_fstat
