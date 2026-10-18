.intel_syntax noprefix

.text

.global sys_close
.type sys_close, @function
sys_close:
	mov eax, 0x3
	syscall
	ret

.size sys_close, .-sys_close
