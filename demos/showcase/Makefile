# ProGearSDK showcase Makefile

# === Configuration ===
GAME_NAME = mygame

# MAME configuration - uses puzzledp driver for homebrew testing
MAME_DRIVER = puzzledp
NEOGEO_BIOS_PATH = ../../../bios

# Toolchain
PREFIX = m68k-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy

# Z80 assembler (sdasz80 from SDCC)
Z80ASM = sdasz80

# Paths
SDK_PATH = ../../sdk
TOOLS_PATH = ../../tools
BUILD_DIR = build

# Asset pipeline
NGRES = python3 $(TOOLS_PATH)/ngres.py
ASSETS_YAML = assets.yaml
SDK_ASSETS = $(SDK_PATH)/assets/assets.yaml
ASSETS_DIR = assets
GEN_DIR = $(BUILD_DIR)/gen

# === Compiler Flags ===
CFLAGS = -m68000 -Os -fomit-frame-pointer -ffreestanding
CFLAGS += -Wall -Wextra -Wshadow -Wdouble-promotion -Wformat=2 -Wundef
CFLAGS += -fno-common -Wconversion -Wno-sign-conversion
CFLAGS += -I$(SDK_PATH)/include -I$(GEN_DIR)

ASFLAGS = -m68000

LDFLAGS = -T$(SDK_PATH)/rom/link.ld -nostdlib

# === Source Files ===
# Game sources organized by demo
GAME_SOURCES = src/main.c \
               src/ball/ball.c \
               src/ball/ball_demo.c \
               src/scroll/scroll_demo.c \
               src/blank_scene/blank_scene.c \
               src/tilemap_demo/tilemap_demo.c

SDK_C_SOURCES = $(SDK_PATH)/src/fix.c $(SDK_PATH)/src/color.c $(SDK_PATH)/src/palette.c $(SDK_PATH)/src/scene.c $(SDK_PATH)/src/actor.c $(SDK_PATH)/src/parallax.c $(SDK_PATH)/src/input.c $(SDK_PATH)/src/ngmath.c $(SDK_PATH)/src/physics.c $(SDK_PATH)/src/camera.c $(SDK_PATH)/src/arena.c $(SDK_PATH)/src/spring.c $(SDK_PATH)/src/ui.c $(SDK_PATH)/src/audio.c $(SDK_PATH)/src/engine.c $(SDK_PATH)/src/tilemap.c
ASM_SOURCES = $(SDK_PATH)/src/crt0.s

# Object files go in build directory
OBJECTS = $(BUILD_DIR)/main.o \
          $(BUILD_DIR)/ball.o \
          $(BUILD_DIR)/ball_demo.o \
          $(BUILD_DIR)/scroll_demo.o \
          $(BUILD_DIR)/blank_scene.o \
          $(BUILD_DIR)/tilemap_demo.o \
          $(BUILD_DIR)/fix.o \
          $(BUILD_DIR)/color.o \
          $(BUILD_DIR)/palette.o \
          $(BUILD_DIR)/scene.o \
          $(BUILD_DIR)/actor.o \
          $(BUILD_DIR)/parallax.o \
          $(BUILD_DIR)/input.o \
          $(BUILD_DIR)/ngmath.o \
          $(BUILD_DIR)/physics.o \
          $(BUILD_DIR)/camera.o \
          $(BUILD_DIR)/arena.o \
          $(BUILD_DIR)/spring.o \
          $(BUILD_DIR)/ui.o \
          $(BUILD_DIR)/audio.o \
          $(BUILD_DIR)/engine.o \
          $(BUILD_DIR)/tilemap.o \
          $(BUILD_DIR)/crt0.o

# === ROM Output Files (in build directory) ===
P_ROM = $(BUILD_DIR)/$(GAME_NAME)-p1.bin
M_ROM = $(BUILD_DIR)/$(GAME_NAME)-m1.bin
S_ROM = $(BUILD_DIR)/$(GAME_NAME)-s1.bin
C1_ROM = $(BUILD_DIR)/$(GAME_NAME)-c1.bin
C2_ROM = $(BUILD_DIR)/$(GAME_NAME)-c2.bin
V1_ROM = $(BUILD_DIR)/$(GAME_NAME)-v1.bin
ELF_FILE = $(BUILD_DIR)/$(GAME_NAME).elf

# Generated files from asset pipeline
GEN_ASSETS_H = $(GEN_DIR)/ngres_generated_assets.h
GEN_C1 = $(GEN_DIR)/sprites-c1.bin
GEN_C2 = $(GEN_DIR)/sprites-c2.bin
GEN_V1 = $(GEN_DIR)/audio-v1.bin
GEN_M1_TABLES = $(GEN_DIR)/audio-tables.bin

# === Build Rules ===
.PHONY: all clean mame assets

all: $(P_ROM) $(M_ROM) $(S_ROM) $(C1_ROM) $(C2_ROM) $(V1_ROM)
	@echo ""
	@echo "Build complete!"
	@echo "ROM files:"
	@ls -la $(BUILD_DIR)/$(GAME_NAME)-*.bin

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(GEN_DIR): | $(BUILD_DIR)
	@mkdir -p $(GEN_DIR)

# Asset pipeline - process sprites from YAML
assets: $(GEN_ASSETS_H)

# Generate sprite and audio data from YAML if assets.yaml exists
$(GEN_ASSETS_H): | $(GEN_DIR)
	@if [ -f $(ASSETS_YAML) ]; then \
		echo "Processing assets..."; \
		$(NGRES) --sdk-assets $(SDK_ASSETS) $(ASSETS_YAML) -o $(GEN_DIR) --c1 sprites-c1.bin --c2 sprites-c2.bin --v1 audio-v1.bin --m1-tables audio-tables.bin -v; \
	else \
		echo "No $(ASSETS_YAML) found, creating empty ngres_generated_assets.h"; \
		echo "// ngres_generated_assets.h - No assets defined" > $(GEN_ASSETS_H); \
		echo "#ifndef _NGRES_GENERATED_ASSETS_H_" >> $(GEN_ASSETS_H); \
		echo "#define _NGRES_GENERATED_ASSETS_H_" >> $(GEN_ASSETS_H); \
		echo "#include <audio.h>" >> $(GEN_ASSETS_H); \
		echo "#endif" >> $(GEN_ASSETS_H); \
	fi

# Compile game source files (depend on generated headers)
$(BUILD_DIR)/main.o: src/main.c $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc -c $< -o $@

$(BUILD_DIR)/ball.o: src/ball/ball.c src/ball/ball.h $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc -Isrc/ball -c $< -o $@

$(BUILD_DIR)/ball_demo.o: src/ball/ball_demo.c src/ball/ball_demo.h $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc -Isrc/ball -c $< -o $@

$(BUILD_DIR)/scroll_demo.o: src/scroll/scroll_demo.c src/scroll/scroll_demo.h $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc -Isrc/scroll -c $< -o $@

$(BUILD_DIR)/blank_scene.o: src/blank_scene/blank_scene.c src/blank_scene/blank_scene.h $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc -Isrc/blank_scene -c $< -o $@

$(BUILD_DIR)/tilemap_demo.o: src/tilemap_demo/tilemap_demo.c src/tilemap_demo/tilemap_demo.h $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc -Isrc/tilemap_demo -c $< -o $@

# Compile SDK C files
$(BUILD_DIR)/fix.o: $(SDK_PATH)/src/fix.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/color.o: $(SDK_PATH)/src/color.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/palette.o: $(SDK_PATH)/src/palette.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/scene.o: $(SDK_PATH)/src/scene.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/actor.o: $(SDK_PATH)/src/actor.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/parallax.o: $(SDK_PATH)/src/parallax.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/input.o: $(SDK_PATH)/src/input.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ngmath.o: $(SDK_PATH)/src/ngmath.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/physics.o: $(SDK_PATH)/src/physics.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/camera.o: $(SDK_PATH)/src/camera.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arena.o: $(SDK_PATH)/src/arena.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/spring.o: $(SDK_PATH)/src/spring.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ui.o: $(SDK_PATH)/src/ui.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/audio.o: $(SDK_PATH)/src/audio.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/engine.o: $(SDK_PATH)/src/engine.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tilemap.o: $(SDK_PATH)/src/tilemap.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble crt0
$(BUILD_DIR)/crt0.o: $(SDK_PATH)/src/crt0.s | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Link ELF (use gcc to automatically include libgcc for division/modulo)
$(ELF_FILE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -lgcc -o $@

# Extract P-ROM binary (with byte-swap for MAME compatibility)
$(P_ROM): $(ELF_FILE)
	$(OBJCOPY) -O binary -S $< $@
	dd if=$@ of=$@ conv=notrunc,swab status=none
	truncate -s 128K $@ 2>/dev/null || dd if=/dev/null of=$@ bs=1 seek=131072 count=0

# Build M-ROM (Z80) with audio sample tables
$(M_ROM): $(SDK_PATH)/z80/driver.s $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(Z80ASM) -o $(BUILD_DIR)/driver.rel $<
	sdld -n -i $(BUILD_DIR)/driver.ihx -b _CODE=0x0000 -b _DATA=0xF800 $(BUILD_DIR)/driver.rel
	sdobjcopy -I ihex -O binary $(BUILD_DIR)/driver.ihx $@
	@# Patch audio sample tables at offset 0x0800 if they exist
	@if [ -f $(GEN_M1_TABLES) ]; then \
		dd if=$(GEN_M1_TABLES) of=$@ bs=1 seek=2048 conv=notrunc status=none 2>/dev/null || true; \
	fi
	truncate -s 64K $@ 2>/dev/null || dd if=/dev/null of=$@ bs=1 seek=65536 count=0
	rm -f $(BUILD_DIR)/driver.rel $(BUILD_DIR)/driver.ihx $(BUILD_DIR)/driver.sym $(BUILD_DIR)/driver.map $(BUILD_DIR)/driver.noi

# Copy/generate S-ROM
$(S_ROM): $(SDK_PATH)/rom/sfix.bin | $(BUILD_DIR)
	cp $< $@

# C-ROM from asset pipeline (or preserve existing, or create empty)
$(C1_ROM): $(GEN_ASSETS_H) | $(BUILD_DIR)
	@if [ -f $(GEN_C1) ]; then \
		cp $(GEN_C1) $@; \
	elif [ ! -f $@ ]; then \
		dd if=/dev/zero of=$@ bs=1K count=64 2>/dev/null; \
	fi

$(C2_ROM): $(GEN_ASSETS_H) | $(BUILD_DIR)
	@if [ -f $(GEN_C2) ]; then \
		cp $(GEN_C2) $@; \
	elif [ ! -f $@ ]; then \
		dd if=/dev/zero of=$@ bs=1K count=64 2>/dev/null; \
	fi

# V-ROM from asset pipeline (or create empty)
$(V1_ROM): $(GEN_ASSETS_H) | $(BUILD_DIR)
	@if [ -f $(GEN_V1) ]; then \
		cp $(GEN_V1) $@; \
	elif [ ! -f $@ ]; then \
		dd if=/dev/zero of=$@ bs=1K count=512 2>/dev/null; \
	fi

# === MAME Target ===
# Creates puzzledp.zip with proper ROM names/sizes and runs in MAME
mame: all $(BUILD_DIR)/$(MAME_DRIVER).zip
	@echo "Running in MAME..."
	mame $(MAME_DRIVER) -rompath "$(BUILD_DIR);$(NEOGEO_BIOS_PATH)" -nofilter -effect scanlines -window -resolution 640x448 -skip_gameinfo

$(BUILD_DIR)/$(MAME_DRIVER).zip: $(P_ROM) $(M_ROM) $(S_ROM) $(C1_ROM) $(C2_ROM) $(V1_ROM)
	@echo "Creating $(MAME_DRIVER).zip for MAME..."
	@rm -rf $(BUILD_DIR)/$(MAME_DRIVER)_tmp
	@mkdir -p $(BUILD_DIR)/$(MAME_DRIVER)_tmp
	@# P-ROM: 512KB
	@cp $(P_ROM) $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-p1.p1
	@truncate -s 524288 $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-p1.p1
	@# S-ROM: 128KB
	@cp $(S_ROM) $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-s1.s1
	@truncate -s 131072 $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-s1.s1
	@# M-ROM: 128KB
	@cp $(M_ROM) $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-m1.m1
	@truncate -s 131072 $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-m1.m1
	@# V-ROM: 2MB (audio samples and music)
	@cp $(V1_ROM) $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-v1.v1
	@truncate -s 2097152 $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-v1.v1
	@# C-ROM: 1MB each (sprites)
	@cp $(C1_ROM) $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-c1.c1
	@truncate -s 1048576 $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-c1.c1
	@cp $(C2_ROM) $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-c2.c2
	@truncate -s 1048576 $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-c2.c2
	@cd $(BUILD_DIR)/$(MAME_DRIVER)_tmp && zip -q ../$(MAME_DRIVER).zip *
	@rm -rf $(BUILD_DIR)/$(MAME_DRIVER)_tmp
	@echo "Created $(BUILD_DIR)/$(MAME_DRIVER).zip"

clean:
	rm -rf $(BUILD_DIR)

# === Code Quality Targets ===
# All C source files (SDK + demo)
ALL_C_SOURCES = $(SDK_C_SOURCES) $(GAME_SOURCES)
ALL_H_SOURCES = $(wildcard $(SDK_PATH)/include/*.h) $(wildcard src/**/*.h)

.PHONY: format format-check lint check

# Auto-format all source files
format:
	@echo "Formatting source files..."
	@clang-format -i $(ALL_C_SOURCES) $(ALL_H_SOURCES)
	@echo "Done."

# Check formatting without modifying (fails if formatting needed)
format-check:
	@echo "Checking code formatting..."
	@clang-format --dry-run --Werror $(ALL_C_SOURCES) $(ALL_H_SOURCES)
	@echo "Formatting OK."

# Static analysis with cppcheck
lint:
	@echo "Running static analysis..."
	@cppcheck --enable=warning,performance,portability \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction \
		--error-exitcode=1 \
		--inline-suppr \
		-I$(SDK_PATH)/include \
		$(ALL_C_SOURCES)
	@echo "Static analysis OK."

# Run all checks (for CI)
check: format-check lint
	@echo "All checks passed."
