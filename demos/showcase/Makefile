# ProGearSDK Showcase Makefile

# === Configuration ===
GAME_NAME = mygame

# MAME configuration - uses puzzledp driver for homebrew testing
MAME_DRIVER = puzzledp
NEOGEO_BIOS_PATH = ../../bios

# === SDK Setup ===
SDK_PATH = ../../sdk
TOOLS_PATH = ../../tools
include $(SDK_PATH)/sdk.mk

# === Paths ===
BUILD_DIR = build
GEN_DIR = $(BUILD_DIR)/gen

# Asset pipeline
PROGEAR_ASSETS = python3 $(TOOLS_PATH)/progear_assets.py
NEO_ROM = python3 $(TOOLS_PATH)/neo_rom.py
ASSETS_YAML = assets.yaml
SDK_ASSETS = $(SDK_PATH)/assets/assets.yaml

# NeoSD .neo file metadata (override as needed)
NEO_NAME ?= $(GAME_NAME)
NEO_MANU ?= Homebrew
NEO_YEAR ?= 2024
NEO_NGH ?= 9999

# === Compiler Flags ===
CFLAGS = $(SDK_CFLAGS) -I$(GEN_DIR) -Isrc

# === Source Files ===
GAME_SOURCES = src/main.c \
               src/ball/ball.c \
               src/ball/ball_demo.c \
               src/scroll/scroll_demo.c \
               src/blank_scene/blank_scene.c \
               src/tilemap_demo/tilemap_demo.c

# Object files
GAME_OBJECTS = $(BUILD_DIR)/main.o \
               $(BUILD_DIR)/ball.o \
               $(BUILD_DIR)/ball_demo.o \
               $(BUILD_DIR)/scroll_demo.o \
               $(BUILD_DIR)/blank_scene.o \
               $(BUILD_DIR)/tilemap_demo.o

# === ROM Output Files ===
P_ROM = $(BUILD_DIR)/$(GAME_NAME)-p1.bin
M_ROM = $(BUILD_DIR)/$(GAME_NAME)-m1.bin
S_ROM = $(BUILD_DIR)/$(GAME_NAME)-s1.bin
C1_ROM = $(BUILD_DIR)/$(GAME_NAME)-c1.bin
C2_ROM = $(BUILD_DIR)/$(GAME_NAME)-c2.bin
V1_ROM = $(BUILD_DIR)/$(GAME_NAME)-v1.bin
NEO_FILE = $(BUILD_DIR)/$(GAME_NAME).neo
ELF_FILE = $(BUILD_DIR)/$(GAME_NAME).elf

# Generated files from asset pipeline
GEN_ASSETS_H = $(GEN_DIR)/progear_assets.h
GEN_C1 = $(GEN_DIR)/sprites-c1.bin
GEN_C2 = $(GEN_DIR)/sprites-c2.bin
GEN_V1 = $(GEN_DIR)/audio-v1.bin
GEN_M1_TABLES = $(GEN_DIR)/audio-tables.bin

# === Build Rules ===
.PHONY: all clean mame neo assets sdk

all: sdk $(P_ROM) $(M_ROM) $(S_ROM) $(C1_ROM) $(C2_ROM) $(V1_ROM)
	@echo ""
	@echo "Build complete!"
	@echo "ROM files:"
	@ls -la $(BUILD_DIR)/$(GAME_NAME)-*.bin

# Create build directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(GEN_DIR): | $(BUILD_DIR)
	@mkdir -p $(GEN_DIR)

# Asset pipeline - process sprites from YAML
assets: $(GEN_ASSETS_H)

$(GEN_ASSETS_H): | $(GEN_DIR)
	@if [ -f $(ASSETS_YAML) ]; then \
		echo "Processing assets..."; \
		$(PROGEAR_ASSETS) --sdk-assets $(SDK_ASSETS) $(ASSETS_YAML) -o $(GEN_DIR) --c1 sprites-c1.bin --c2 sprites-c2.bin --v1 audio-v1.bin --m1-tables audio-tables.bin -v; \
	else \
		echo "No $(ASSETS_YAML) found, creating empty progear_assets.h"; \
		echo "// progear_assets.h - No assets defined" > $(GEN_ASSETS_H); \
		echo "#ifndef _PROGEAR_ASSETS_H_" >> $(GEN_ASSETS_H); \
		echo "#define _PROGEAR_ASSETS_H_" >> $(GEN_ASSETS_H); \
		echo "#include <audio.h>" >> $(GEN_ASSETS_H); \
		echo "#endif" >> $(GEN_ASSETS_H); \
	fi

# === Compile Game Sources ===
$(BUILD_DIR)/main.o: src/main.c $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ball.o: src/ball/ball.c $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc/ball -c $< -o $@

$(BUILD_DIR)/ball_demo.o: src/ball/ball_demo.c $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc/ball -c $< -o $@

$(BUILD_DIR)/scroll_demo.o: src/scroll/scroll_demo.c $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc/scroll -c $< -o $@

$(BUILD_DIR)/blank_scene.o: src/blank_scene/blank_scene.c $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc/blank_scene -c $< -o $@

$(BUILD_DIR)/tilemap_demo.o: src/tilemap_demo/tilemap_demo.c $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc/tilemap_demo -c $< -o $@

# === Link ELF ===
$(ELF_FILE): $(GAME_OBJECTS) $(SDK_LIB) $(SDK_CRT0)
	$(CC) $(SDK_CFLAGS) $(SDK_LDFLAGS) $(SDK_CRT0) $(GAME_OBJECTS) $(SDK_LIB) -lgcc -o $@

# === ROM Generation ===

# P-ROM (with byte-swap for MAME compatibility)
$(P_ROM): $(ELF_FILE)
	$(OBJCOPY) -O binary -S $< $@
	dd if=$@ of=$@ conv=notrunc,swab status=none
	truncate -s 128K $@ 2>/dev/null || dd if=/dev/null of=$@ bs=1 seek=131072 count=0

# M-ROM (Z80) with audio sample tables
$(M_ROM): $(SDK_Z80_DRIVER) $(GEN_ASSETS_H) | $(BUILD_DIR)
	$(Z80ASM) -o $(BUILD_DIR)/driver.rel $<
	sdld -n -i $(BUILD_DIR)/driver.ihx -b _CODE=0x0000 -b _DATA=0xF800 $(BUILD_DIR)/driver.rel
	sdobjcopy -I ihex -O binary $(BUILD_DIR)/driver.ihx $@
	@if [ -f $(GEN_M1_TABLES) ]; then \
		dd if=$(GEN_M1_TABLES) of=$@ bs=1 seek=2048 conv=notrunc status=none 2>/dev/null || true; \
	fi
	truncate -s 64K $@ 2>/dev/null || dd if=/dev/null of=$@ bs=1 seek=65536 count=0
	rm -f $(BUILD_DIR)/driver.rel $(BUILD_DIR)/driver.ihx $(BUILD_DIR)/driver.sym $(BUILD_DIR)/driver.map $(BUILD_DIR)/driver.noi

# S-ROM
$(S_ROM): $(SDK_SFIX) | $(BUILD_DIR)
	cp $< $@

# C-ROM from asset pipeline
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

# V-ROM from asset pipeline
$(V1_ROM): $(GEN_ASSETS_H) | $(BUILD_DIR)
	@if [ -f $(GEN_V1) ]; then \
		cp $(GEN_V1) $@; \
	elif [ ! -f $@ ]; then \
		dd if=/dev/zero of=$@ bs=1K count=512 2>/dev/null; \
	fi

# === NeoSD .neo Target ===
neo: all $(NEO_FILE)
	@echo ""
	@echo "NeoSD ROM created: $(NEO_FILE)"
	@ls -la $(NEO_FILE)

$(NEO_FILE): $(P_ROM) $(M_ROM) $(S_ROM) $(C1_ROM) $(C2_ROM) $(V1_ROM)
	@echo "Creating NeoSD .neo file..."
	$(NEO_ROM) -o $@ \
		--p1 $(P_ROM) \
		--s1 $(S_ROM) \
		--m1 $(M_ROM) \
		--v1 $(V1_ROM) \
		--c1 $(C1_ROM) \
		--c2 $(C2_ROM) \
		--name "$(NEO_NAME)" \
		--manufacturer "$(NEO_MANU)" \
		--year $(NEO_YEAR) \
		--ngh $(NEO_NGH) \
		-v

# === MAME Target ===
mame: all $(BUILD_DIR)/$(MAME_DRIVER).zip
	@echo "Running in MAME..."
	mame $(MAME_DRIVER) -rompath "$(BUILD_DIR);$(NEOGEO_BIOS_PATH)" -nofilter -effect scanlines -window -resolution 640x448 -skip_gameinfo

$(BUILD_DIR)/$(MAME_DRIVER).zip: $(P_ROM) $(M_ROM) $(S_ROM) $(C1_ROM) $(C2_ROM) $(V1_ROM)
	@echo "Creating $(MAME_DRIVER).zip for MAME..."
	@rm -rf $(BUILD_DIR)/$(MAME_DRIVER)_tmp
	@mkdir -p $(BUILD_DIR)/$(MAME_DRIVER)_tmp
	@cp $(P_ROM) $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-p1.p1
	@truncate -s 524288 $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-p1.p1
	@cp $(S_ROM) $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-s1.s1
	@truncate -s 131072 $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-s1.s1
	@cp $(M_ROM) $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-m1.m1
	@truncate -s 131072 $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-m1.m1
	@cp $(V1_ROM) $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-v1.v1
	@truncate -s 2097152 $(BUILD_DIR)/$(MAME_DRIVER)_tmp/202-v1.v1
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
GAME_H_SOURCES = $(wildcard src/*.h) $(wildcard src/**/*.h)

.PHONY: format format-check lint check

format:
	@echo "Formatting game source files..."
	@clang-format -i $(GAME_SOURCES) $(GAME_H_SOURCES)
	@echo "Done."

format-check:
	@echo "Checking game code formatting..."
	@clang-format --dry-run --Werror $(GAME_SOURCES) $(GAME_H_SOURCES)
	@echo "Formatting OK."

lint:
	@echo "Running static analysis on game code..."
	@cppcheck --enable=warning,performance,portability \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction \
		--error-exitcode=1 \
		--inline-suppr \
		-I$(SDK_INCLUDE) -I$(GEN_DIR) -Isrc \
		$(GAME_SOURCES)
	@echo "Static analysis OK."

check: format-check lint
	@echo "All game checks passed."

# Run all checks including SDK
check-all:
	@$(MAKE) -C $(SDK_PATH) check
	@$(MAKE) check
	@echo "All checks passed (SDK + game)."
