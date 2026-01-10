# ProGearSDK Library Makefile
#
# Builds the SDK as a static library and provides code quality targets.
#
# Targets:
#   all          - Build libprogearsdk.a
#   clean        - Remove build artifacts
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
C_SOURCES = $(SRC_DIR)/fix.c \
            $(SRC_DIR)/color.c \
            $(SRC_DIR)/palette.c \
            $(SRC_DIR)/scene.c \
            $(SRC_DIR)/actor.c \
            $(SRC_DIR)/parallax.c \
            $(SRC_DIR)/input.c \
            $(SRC_DIR)/ngmath.c \
            $(SRC_DIR)/physics.c \
            $(SRC_DIR)/camera.c \
            $(SRC_DIR)/arena.c \
            $(SRC_DIR)/spring.c \
            $(SRC_DIR)/ui.c \
            $(SRC_DIR)/audio.c \
            $(SRC_DIR)/engine.c \
            $(SRC_DIR)/tilemap.c

ASM_SOURCES = $(SRC_DIR)/crt0.s

H_SOURCES = $(wildcard $(INC_DIR)/*.h)

# Object files
C_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS = $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/%.o,$(ASM_SOURCES))
OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

# === Build Rules ===
.PHONY: all clean format format-check lint check

all: $(LIBRARY)
	@echo "SDK build complete: $(LIBRARY)"

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Build static library
$(LIBRARY): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

# Compile C files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble assembly files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Header dependencies (simplified - rebuild all if any header changes)
$(C_OBJECTS): $(H_SOURCES)

clean:
	rm -rf $(BUILD_DIR)

# === Code Quality Targets ===

# Auto-format all source files
format:
	@echo "Formatting SDK source files..."
	@clang-format -i $(C_SOURCES) $(H_SOURCES)
	@echo "Done."

# Check formatting without modifying (fails if formatting needed)
format-check:
	@echo "Checking SDK code formatting..."
	@clang-format --dry-run --Werror $(C_SOURCES) $(H_SOURCES)
	@echo "Formatting OK."

# Static analysis with cppcheck
lint:
	@echo "Running static analysis on SDK..."
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
	@echo "All SDK checks passed."
