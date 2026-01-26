# NeoGeo HAL (Hardware Abstraction Layer) Makefile
#
# Builds the HAL as a static library plus NeoGeo runtime components.
# Depends on the Core library for types, math, and arena allocator.
#
# Targets:
#   all          - Build libneogeo.a and crt0.o
#   clean        - Remove build artifacts
#   format       - Auto-format all HAL source files
#   format-check - Check formatting (fails if changes needed)
#   lint         - Run static analysis with cppcheck
#   check        - Run all checks (format-check + lint)

# === Toolchain ===
# Can be overridden via environment or config.mk
PREFIX ?= m68k-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
AR = $(PREFIX)ar

# === Paths ===
BUILD_DIR = build
SRC_DIR = src
INC_DIR = include
STARTUP_DIR = startup
Z80_DIR = z80
ROM_DIR = rom
CORE_INC_DIR = ../core/include

# === Output ===
LIBRARY = $(BUILD_DIR)/libneogeo.a
CRT0 = $(BUILD_DIR)/crt0.o

# === Compiler Flags ===
CFLAGS = -m68000 -Os -fomit-frame-pointer -ffreestanding
CFLAGS += -Wall -Wextra -Wshadow -Wdouble-promotion -Wformat=2 -Wundef
CFLAGS += -fno-common -Wconversion -Wno-sign-conversion
CFLAGS += -I$(INC_DIR) -I$(CORE_INC_DIR)

ASFLAGS = -m68000

# === Source Files ===
C_SOURCES = $(SRC_DIR)/ng_color.c \
            $(SRC_DIR)/ng_palette.c \
            $(SRC_DIR)/ng_sprite.c \
            $(SRC_DIR)/ng_fix.c \
            $(SRC_DIR)/ng_input.c \
            $(SRC_DIR)/ng_audio.c \
            $(SRC_DIR)/ng_interrupt.c \
            $(SRC_DIR)/ng_system.c \
            $(SRC_DIR)/ng_sram.c \
            $(SRC_DIR)/ng_memcard.c \
            $(SRC_DIR)/ng_bios.c

H_SOURCES = $(wildcard $(INC_DIR)/*.h)

# Object files
C_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
OBJECTS = $(C_OBJECTS)

# === Build Rules ===
.PHONY: all clean format format-check lint check

all: $(LIBRARY) $(CRT0)
	@echo "HAL build complete: $(LIBRARY) $(CRT0)"

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

# Build startup code (not included in library - linked directly by apps)
$(CRT0): $(STARTUP_DIR)/crt0.s | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR)

# === Code Quality Targets ===

# Auto-format all source files
format:
	@echo "Formatting HAL source files..."
	@clang-format -i $(C_SOURCES) $(H_SOURCES)
	@echo "Done."

# Check formatting without modifying (fails if formatting needed)
format-check:
	@echo "Checking HAL code formatting..."
	@clang-format --dry-run --Werror $(C_SOURCES) $(H_SOURCES)
	@echo "Formatting OK."

# Static analysis with cppcheck
# Note: __CPPCHECK__ is defined to enable compatible fallbacks for GCC-specific
# syntax (inline assembly, register variables) that cppcheck cannot parse.
lint:
	@echo "Running static analysis on HAL..."
	@cppcheck --enable=warning,performance,portability \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction \
		--error-exitcode=1 \
		--inline-suppr \
		-D__CPPCHECK__ \
		-I$(INC_DIR) -I$(CORE_INC_DIR) \
		$(C_SOURCES)
	@echo "Static analysis OK."

# Run all checks
check: format-check lint
	@echo "All HAL checks passed."
