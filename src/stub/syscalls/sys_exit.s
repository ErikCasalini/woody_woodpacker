.intel_syntax noprefix

.text

.global sys_exit
.type sys_exit, @function
sys_exit:
	mov eax, 0x3C
	syscall
	ud2				# Triggers an invalid instruction exception, beacuse it should not be normal to reach this

.size sys_exit, .-sys_exit
