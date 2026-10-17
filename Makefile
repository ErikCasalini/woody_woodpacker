CC := clang
LD := ld

LINKER_SCRIPT := src/stub/linker_script

STUB_OBJ_DIR := stub_bin
STUB := $(STUB_OBJ_DIR)/stub.elf
STUB_CFLAGS := -Wall -Werror -Wextra -ffreestanding -fno-stack-protector -nostdlib -fPIE -MMD
# -Xassembler tels Clang to send the following option to the assembler tool
STUB_ASFLAGS := -Xassembler --fatal-warnings
STUB_LDFLAGS := -static --no-dynamic-linker -T $(LINKER_SCRIPT)

PACKER_OBJ_DIR := packer_bin
PACKER := packer
PACKER_CFLAGS := -Wall -Werror -Wextra -g
# PACKER_ASFLAGS := -Wa,--fatal-warnings
# PACKER_LDFLAGS := -Wl,-Ttext-segment=0x10000

COMMON_SRC := src/ft/ft_memcpy.c \
			  src/ft/ft_memmove.c \
			  src/ft/ft_memset.c \
			  src/ft/ft_strlen.c

STUB_SRC := src/stub/execute_packed_elf.c \
			src/stub/entry.s \
			src/stub/jump_to_with_stack.s \
			src/stub/syscalls/sys_close.s \
			src/stub/syscalls/sys_exit.s \
			src/stub/syscalls/sys_fstat.s \
			src/stub/syscalls/sys_lseek.s \
			src/stub/syscalls/sys_mmap.s \
			src/stub/syscalls/sys_mprotect.s \
			src/stub/syscalls/sys_munmap.s \
			src/stub/syscalls/sys_open.s \
			src/stub/syscalls/sys_write.s

PACKER_SRC := src/packer/main.c \
			  src/ft/ft_strncmp.c

STUB_ALL_SRC := $(STUB_SRC) $(COMMON_SRC)
PACKER_ALL_SRC := $(PACKER_SRC) $(COMMON_SRC)

STUB_OBJ := $(addprefix $(STUB_OBJ_DIR)/, $(STUB_ALL_SRC:src/%=%))
STUB_OBJ := $(STUB_OBJ:.c=.o)
STUB_OBJ := $(STUB_OBJ:.s=.o)
STUB_DEPS := $(STUB_OBJ:.o=.d)

PACKER_OBJ := $(addprefix $(PACKER_OBJ_DIR)/, $(PACKER_ALL_SRC:src/%=%))
PACKER_OBJ := $(PACKER_OBJ:.c=.o)
# PACKER_OBJ := $(PACKER_OBJ:.s=.o)
PACKER_DEPS := $(PACKER_OBJ:.o=.d)

all: $(PACKER)

stub: $(STUB)

$(STUB): $(STUB_OBJ) $(LINKER_SCRIPT)
	$(LD) $(STUB_OBJ) $(STUB_LDFLAGS) -o $@

$(PACKER): $(PACKER_OBJ) $(STUB)
	$(CC) $(PACKER_OBJ) -o $@
	objcopy --dump-section .stub=stub.bin \
    		$(STUB)
	objcopy --add-section .stub=stub.bin \
			$@
	@rm stub.bin

# $(@D) -> parent directory of target
$(STUB_OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(STUB_CFLAGS) -c $< -o $@

$(STUB_OBJ_DIR)/%.o: src/%.s
	@mkdir -p $(@D)
	$(CC) $(STUB_ASFLAGS) -c $< -o $@

$(PACKER_OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(PACKER_CFLAGS) -c $< -o $@

# $(PACKER_OBJ_DIR)/%.o: src/%.s
#	mkdir -p $(@D)
#	$(CC) $(PACKER_ASFLAGS) -c $< -o $@

-include $(STUB_DEPS)
-include $(PACKER_DEPS)

clean:
	rm -rf $(STUB_OBJ_DIR)
	rm -rf $(PACKER_OBJ_DIR)

fclean: clean
	rm -f $(PACKER)

re:
	$(MAKE) fclean
	$(MAKE) all

.PHONY: all stub clean fclean re
