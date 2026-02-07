TARGET = crefl_demo
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c17 -Isrc/include -g -m64 -MMD -MP

SRC_DIR = src
DEMO_DIR = demo
OBJ_DIR = build/obj
BIN_DIR = build/bin

# --- Source Discovery ---
# This looks for ALL .c files in your src directory automatically.
# If you prefer manual listing, ensure the path matches the folder exactly.
C_FILES := $(shell find $(SRC_DIR) -name "*.c")

# --- Object Mapping ---
# This takes src/folder/file.c and turns it into build/obj/folder/file.o
OBJ   := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(C_FILES))
DEPS  := $(OBJ:.o=.d)

BIN = $(BIN_DIR)/$(TARGET)

# --- Rules ---

all: $(BIN)

$(BIN): $(OBJ) | $(BIN_DIR)
	@echo "Linking -> $@"
	$(CC) $(CFLAGS) -o $@ $(OBJ)

# Rule for C files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling -> $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for ASM files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	@echo "Assembling -> $<"
	nasm -f elf64 $< -o $@

-include $(DEPS)

$(BIN_DIR):
	@mkdir -p $@

# --- Run ---
run: all
	@echo "Running $(TARGET)..."
	@./$(BIN)

clean:
	rm -rf build

.PHONY: all clean run