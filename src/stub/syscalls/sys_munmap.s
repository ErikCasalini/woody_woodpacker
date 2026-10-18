.intel_syntax noprefix

.text

.global sys_munmap
.type sys_munmap, @function
sys_munmap:
	mov eax, 0xB
	syscall
	ret

.size sys_munmap, .-sys_munmap
