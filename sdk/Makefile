# ProGearSDK Library Makefile
#
# Builds the SDK as a static library and provides code quality targets.
#
# Targets:
#   all          - Build libprogearsdk.a
#   clean        - Remove build artifacts
#   docs         - Generate API documentation with Doxygen
#   format       - Auto-format all SDK source files
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

# === Output ===
LIBRARY = $(BUILD_DIR)/libprogearsdk.a

# === Compiler Flags ===
CFLAGS = -m68000 -Os -fomit-frame-pointer -ffreestanding
CFLAGS += -Wall -Wextra -Wshadow -Wdouble-promotion -Wformat=2 -Wundef
CFLAGS += -fno-common -Wconversion -Wno-sign-conversion
CFLAGS += -I$(INC_DIR)

ASFLAGS = -m68000

# === Source Files ===
# Hardware layer (internal)
HW_SOURCES = $(SRC_DIR)/hw/system.c \
             $(SRC_DIR)/hw/sprite.c \
             $(SRC_DIR)/hw/palette.c \
             $(SRC_DIR)/hw/fix.c \
             $(SRC_DIR)/hw/input.c \
             $(SRC_DIR)/hw/audio.c

# Public API implementations
C_SOURCES = $(SRC_DIR)/text.c \
            $(SRC_DIR)/color.c \
            $(SRC_DIR)/palette.c \
            $(SRC_DIR)/fix.c \
            $(SRC_DIR)/lighting.c \
            $(SRC_DIR)/scene.c \
            $(SRC_DIR)/actor.c \
            $(SRC_DIR)/backdrop.c \
            $(SRC_DIR)/input.c \
            $(SRC_DIR)/ngmath.c \
            $(SRC_DIR)/physics.c \
            $(SRC_DIR)/camera.c \
            $(SRC_DIR)/arena.c \
            $(SRC_DIR)/spring.c \
            $(SRC_DIR)/ui.c \
            $(SRC_DIR)/audio.c \
            $(SRC_DIR)/engine.c \
            $(SRC_DIR)/terrain.c

ASM_SOURCES = $(SRC_DIR)/crt0.s

H_SOURCES = $(wildcard $(INC_DIR)/*.h) $(wildcard $(INC_DIR)/hw/*.h)

# Object files
HW_OBJECTS = $(patsubst $(SRC_DIR)/hw/%.c,$(BUILD_DIR)/hw/%.o,$(HW_SOURCES))
C_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS = $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/%.o,$(ASM_SOURCES))
OBJECTS = $(HW_OBJECTS) $(C_OBJECTS) $(ASM_OBJECTS)

# === Build Rules ===
.PHONY: all clean docs format format-check lint check

all: $(LIBRARY)
	@echo "SDK build complete: $(LIBRARY)"

# Build static library
$(LIBRARY): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

# Compile hw/ C files (must come before general rule)
$(BUILD_DIR)/hw/%.o: $(SRC_DIR)/hw/%.c
	@mkdir -p $(BUILD_DIR)/hw
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble assembly files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Header dependencies (simplified - rebuild all if any header changes)
$(C_OBJECTS) $(HW_OBJECTS): $(H_SOURCES)

clean:
	rm -rf $(BUILD_DIR)

# === Documentation ===

# Generate API documentation with Doxygen
docs:
	@command -v doxygen >/dev/null 2>&1 || { echo "Error: doxygen not found. Install with: brew install doxygen"; exit 1; }
	@echo "Generating API documentation..."
	@doxygen Doxyfile
	@echo "Documentation generated in docs/"

# === Code Quality Targets ===

# Auto-format all source files
format:
	@echo "Formatting SDK source files..."
	@clang-format -i $(HW_SOURCES) $(C_SOURCES) $(H_SOURCES)
	@echo "Done."

# Check formatting without modifying (fails if formatting needed)
format-check:
	@echo "Checking SDK code formatting..."
	@clang-format --dry-run --Werror $(HW_SOURCES) $(C_SOURCES) $(H_SOURCES)
	@echo "Formatting OK."

# Static analysis with cppcheck
# Note: __CPPCHECK__ is defined to enable compatible fallbacks for GCC-specific
# syntax (inline assembly, register variables) that cppcheck cannot parse.
lint:
	@echo "Running static analysis on SDK..."
	@cppcheck --enable=warning,performance,portability \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction \
		--error-exitcode=1 \
		--inline-suppr \
		-D__CPPCHECK__ \
		-I$(INC_DIR) \
		$(HW_SOURCES) $(C_SOURCES)
	@echo "Static analysis OK."

# Run all checks
check: format-check lint
	@echo "All SDK checks passed."
