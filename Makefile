TARGET = crefl_demo
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c17 -Isrc/include -g -m64 -MMD -MP

SRC_DIR = src
DEMO_DIR = demo
OBJ_DIR = build/obj
BIN_DIR = build/bin

CRELF_FILE = generated_types.crefl
PY_GEN    = python3 crefl_gen/main.py
PY_INPUT  = $(shell find src -name "*.h") $(shell find src -name "*.c")

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

$(CRELF_FILE): $(PY_INPUT)
	@mkdir -p $(dir $@)
	@echo "Generating reflection -> $@"
	$(PY_GEN) -i $(PY_INPUT) -o $@

# Rule for C files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(CRELF_FILE)
	@mkdir -p $(dir $@)
	@echo "Compiling -> $<"
	$(CC) $(CFLAGS) -c $< -o $@ -DCREFL_FILE=\"$(CRELF_FILE)\"

-include $(DEPS)

$(BIN_DIR):
	@mkdir -p $@

# --- Run ---
run: all
	@echo "Running $(TARGET)..."
	@./$(BIN)

clean:
	rm -rf build
	rm generated_types.crefl

.PHONY: all clean run