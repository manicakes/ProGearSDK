# NeoGeo Core Foundation Library Makefile
#
# Builds the core library containing foundational types, math, and memory
# utilities with no hardware dependencies.
#
# Targets:
#   all          - Build libneogeo_core.a
#   clean        - Remove build artifacts
#   format       - Auto-format all source files
#   format-check - Check formatting (fails if changes needed)
#   lint         - Run static analysis with cppcheck
#   check        - Run all checks (format-check + lint)

# === Toolchain ===
# Can be overridden via environment or config.mk
PREFIX ?= m68k-elf-
CC = $(PREFIX)gcc
AR = $(PREFIX)ar

# === Paths ===
BUILD_DIR = build
SRC_DIR = src
INC_DIR = include

# === Output ===
LIBRARY = $(BUILD_DIR)/libneogeo_core.a

# === Compiler Flags ===
CFLAGS = -m68000 -Os -fomit-frame-pointer -ffreestanding
CFLAGS += -Wall -Wextra -Wshadow -Wdouble-promotion -Wformat=2 -Wundef
CFLAGS += -fno-common -Wconversion -Wno-sign-conversion
CFLAGS += -I$(INC_DIR)

# === Source Files ===
C_SOURCES = $(SRC_DIR)/ng_math.c \
            $(SRC_DIR)/ng_arena.c \
            $(SRC_DIR)/ng_string.c

H_SOURCES = $(wildcard $(INC_DIR)/*.h)

# Object files
C_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
OBJECTS = $(C_OBJECTS)

# === Build Rules ===
.PHONY: all clean format format-check lint check

all: $(LIBRARY)
	@echo "Core build complete: $(LIBRARY)"

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Build static library
$(LIBRARY): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

# Compile C files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Header dependencies (simplified - rebuild all if any header changes)
$(C_OBJECTS): $(H_SOURCES)

clean:
	rm -rf $(BUILD_DIR)

# === Code Quality Targets ===

# Auto-format all source files
format:
	@echo "Formatting Core source files..."
	@clang-format -i $(C_SOURCES) $(H_SOURCES)
	@echo "Done."

# Check formatting without modifying (fails if formatting needed)
format-check:
	@echo "Checking Core code formatting..."
	@clang-format --dry-run --Werror $(C_SOURCES) $(H_SOURCES)
	@echo "Formatting OK."

# Static analysis with cppcheck
lint:
	@echo "Running static analysis on Core..."
	@cppcheck --enable=warning,performance,portability \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction \
		--error-exitcode=1 \
		--inline-suppr \
		-I$(INC_DIR) \
		$(C_SOURCES)
	@echo "Static analysis OK."

# Run all checks
check: format-check lint
	@echo "All Core checks passed."
