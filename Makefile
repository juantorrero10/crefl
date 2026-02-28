TARGET      = crefl
CC          = gcc

OPT ?= 0


BASE_CFLAGS = -Wall -Wextra -Werror -std=c17 -Isrc/include -m64 -MMD -MP -g -O$(OPT)
CFLAGS      = $(BASE_CFLAGS) -fPIC
AR          = ar
ARFLAGS     = rcs

SRC_DIR     = src
DEMO_DIR    = src/demo
OBJ_DIR     = build/obj
BIN_DIR     = build/bin
LIB_DIR     = build/lib

CRELF_FILE  = generated_types.crefl
PY_GEN      = python3 crefl_gen
PY_INPUT    = $(shell find src -name "*.h") $(shell find src -name "*.c")

CFLAGS += -DCREFL_FILE=\"$(CRELF_FILE)\"

# --- Source discovery (exclude demo) ---
C_FILES := $(shell find $(SRC_DIR) -name "*.c" ! -path "$(DEMO_DIR)/*")
OBJ     := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(C_FILES))
DEPS    := $(OBJ:.o=.d)

# --- Demo sources ---
DEMO_C_FILES := $(shell find $(DEMO_DIR) -name "*.c")
DEMO_OBJ     := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(DEMO_C_FILES))
DEMO_BIN     := $(BIN_DIR)/crefl_demo

# --- Library outputs ---
SHARED_LIB = $(LIB_DIR)/lib$(TARGET).so
STATIC_LIB = $(LIB_DIR)/lib$(TARGET).a

# =========================================================
# Default target -> shared library
# =========================================================
all: $(SHARED_LIB)

# =========================================================
# Shared library
# =========================================================
$(SHARED_LIB): $(OBJ) | $(LIB_DIR)
	@echo "Linking shared library -> $@"
	$(CC) -shared -o $@ $(OBJ)

# =========================================================
# Static library
# =========================================================
static: $(STATIC_LIB)

$(STATIC_LIB): $(OBJ) | $(LIB_DIR)
	@echo "Creating static library -> $@"
	$(AR) $(ARFLAGS) $@ $(OBJ)

# =========================================================
# Reflection generation (ONLY used by demo)
# =========================================================
$(CRELF_FILE): $(PY_INPUT)
	@echo "Generating reflection -> $@"
	$(PY_GEN) -i $(PY_INPUT) -o $@

# =========================================================
# Object compilation
# =========================================================
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling -> $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(DEMO_BIN): $(CRELF_FILE) $(DEMO_OBJ) $(SHARED_LIB) | $(BIN_DIR)
	@echo "Linking demo -> $@"
	$(CC) $(BASE_CFLAGS) -o $@ $(DEMO_OBJ) \
		-L$(LIB_DIR) -l$(TARGET) \
		-Wl,-rpath,$(LIB_DIR)

# =========================================================
# Run demo
# =========================================================
run_demo: $(DEMO_BIN)
	@echo "Running demo..."
	@$(DEMO_BIN)

# =========================================================
# Directories
# =========================================================
$(BIN_DIR) $(LIB_DIR):
	@mkdir -p $@

# =========================================================
# Clean
# =========================================================
clean:
	rm -rf build
	rm -f $(CRELF_FILE)

-include $(DEPS)

.PHONY: all static clean run_demo
