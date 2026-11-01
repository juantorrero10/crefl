TARGET      = crefl
CC          = gcc

OPT ?= 0


BASE_CFLAGS = -Wall -Wextra -Werror -std=c2x -Isrc/include -m64 -MMD -MP -g -O$(OPT)
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

# --- Serialization tests ---
TEST_INPUT          := $(filter-out $(DEMO_DIR)/demo.c,$(PY_INPUT))
QUERY_DESERIALIZE_SRC      := tests/query_deserialization.c
QUERY_DESERIALIZE_METADATA := build/query_deserialization.crefl
QUERY_DESERIALIZE_BIN      := $(BIN_DIR)/query_deserialization
QUERY_SERIALIZE_SRC        := tests/query_serialize.c
QUERY_SERIALIZE_METADATA   := build/query_serialize.crefl
QUERY_SERIALIZE_BIN        := $(BIN_DIR)/query_serialize
JSON_DESERIALIZE_SRC      := tests/json_deserialization.c
JSON_DESERIALIZE_METADATA := build/json_deserialization.crefl
JSON_DESERIALIZE_BIN      := $(BIN_DIR)/json_deserialization
JSON_SERIALIZE_SRC        := tests/json_serialize.c
JSON_SERIALIZE_METADATA   := build/json_serialize.crefl
JSON_SERIALIZE_BIN        := $(BIN_DIR)/json_serialize

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
build_demo: $(DEMO_BIN)


run_demo: build_demo
	@echo "Running demo..."
	@$(DEMO_BIN)

# =========================================================
# Query test
# =========================================================
$(QUERY_DESERIALIZE_METADATA): $(TEST_INPUT) $(QUERY_DESERIALIZE_SRC) | $(BIN_DIR)
	$(PY_GEN) -i $(TEST_INPUT) $(QUERY_DESERIALIZE_SRC) -o $@

$(QUERY_DESERIALIZE_BIN): $(QUERY_DESERIALIZE_SRC) $(QUERY_DESERIALIZE_METADATA) $(SHARED_LIB) | $(BIN_DIR)
	$(CC) $(BASE_CFLAGS) -DCREFL_FILE=\"$(QUERY_DESERIALIZE_METADATA)\" -o $@ $< \
		-L$(LIB_DIR) -l$(TARGET) \
		-Wl,-rpath,$(LIB_DIR)

test_query_deserialize: $(QUERY_DESERIALIZE_BIN)
	@$(QUERY_DESERIALIZE_BIN)

$(QUERY_SERIALIZE_METADATA): $(TEST_INPUT) $(QUERY_SERIALIZE_SRC) | $(BIN_DIR)
	$(PY_GEN) -i $(TEST_INPUT) $(QUERY_SERIALIZE_SRC) -o $@

$(QUERY_SERIALIZE_BIN): $(QUERY_SERIALIZE_SRC) $(QUERY_SERIALIZE_METADATA) $(SHARED_LIB) | $(BIN_DIR)
	$(CC) $(BASE_CFLAGS) -DCREFL_FILE=\"$(QUERY_SERIALIZE_METADATA)\" -o $@ $< \
		-L$(LIB_DIR) -l$(TARGET) \
		-Wl,-rpath,$(LIB_DIR)

test_query_serialize: $(QUERY_SERIALIZE_BIN)
	@$(QUERY_SERIALIZE_BIN)

$(JSON_DESERIALIZE_METADATA): $(TEST_INPUT) $(JSON_DESERIALIZE_SRC) | $(BIN_DIR)
	$(PY_GEN) -i $(TEST_INPUT) $(JSON_DESERIALIZE_SRC) -o $@

$(JSON_DESERIALIZE_BIN): $(JSON_DESERIALIZE_SRC) $(JSON_DESERIALIZE_METADATA) $(SHARED_LIB) | $(BIN_DIR)
	$(CC) $(BASE_CFLAGS) -DCREFL_FILE=\"$(JSON_DESERIALIZE_METADATA)\" -o $@ $< \
		-L$(LIB_DIR) -l$(TARGET) \
		-Wl,-rpath,$(LIB_DIR)

test_json_deserialize: $(JSON_DESERIALIZE_BIN)
	@$(JSON_DESERIALIZE_BIN)

$(JSON_SERIALIZE_METADATA): $(TEST_INPUT) $(JSON_SERIALIZE_SRC) | $(BIN_DIR)
	$(PY_GEN) -i $(TEST_INPUT) $(JSON_SERIALIZE_SRC) -o $@

$(JSON_SERIALIZE_BIN): $(JSON_SERIALIZE_SRC) $(JSON_SERIALIZE_METADATA) $(SHARED_LIB) | $(BIN_DIR)
	$(CC) $(BASE_CFLAGS) -DCREFL_FILE=\"$(JSON_SERIALIZE_METADATA)\" -o $@ $< \
		-L$(LIB_DIR) -l$(TARGET) \
		-Wl,-rpath,$(LIB_DIR)

test_json_serialize: $(JSON_SERIALIZE_BIN)
	@$(JSON_SERIALIZE_BIN)

test_json: test_json_deserialize test_json_serialize


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

.PHONY: all static clean run_demo build_demo test_query_deserialize test_query_serialize test_json_deserialize test_json_serialize test_json
