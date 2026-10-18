.intel_syntax noprefix

.text

.global sys_open
.type sys_open, @function
sys_open:
	mov eax, 0x2
	syscall
	ret

.size sys_open, .-sys_open
