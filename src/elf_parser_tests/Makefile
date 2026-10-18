NAME := woody_woodpacker

CC := cc

CFLAGS := -Wall -Wextra -Werror -MMD -g

ASFLAGS := -Wa,--fatal-warnings
LINKFLAGS := -Wl,-Ttext-segment=0x10000


OBJ_DIR := ./bin

SRC := main.c \
	   str_enum.c \
	   segment_mapper.c \
	   pt_dynamic.c \
	   stack_fix.c \
	   jump_to_with_stack.s

OBJ := $(addprefix $(OBJ_DIR)/, $(SRC:.c=.o))
OBJ := $(OBJ:.s=.o)

DEPS := $(addprefix $(OBJ_DIR)/, $(SRC:.c=.d))

vpath %.c src
vpath %.h src
vpath %.s src

$(NAME): $(OBJ)
	$(CC) $(OBJ) $(LINKFLAGS) -o $@

-include $(DEPS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.s | $(OBJ_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

all: $(NAME)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
