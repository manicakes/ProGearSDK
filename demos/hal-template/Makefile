# HAL-Only Template Makefile
#
# Demonstrates building a NeoGeo application using only the HAL,
# without the SDK game engine.

# === Configuration ===
GAME_NAME = hal-template

# MAME configuration
MAME_DRIVER = puzzledp
NEOGEO_BIOS_PATH = ../../bios

# === HAL Setup ===
HAL_PATH = ../../hal
include $(HAL_PATH)/hal.mk

# === Paths ===
BUILD_DIR = build

# === Compiler Flags ===
CFLAGS = $(HAL_CFLAGS) -Isrc

# === Source Files ===
SOURCES = src/main.c
OBJECTS = $(BUILD_DIR)/main.o

# === ROM Output Files ===
P_ROM = $(BUILD_DIR)/$(GAME_NAME)-p1.bin
M_ROM = $(BUILD_DIR)/$(GAME_NAME)-m1.bin
S_ROM = $(BUILD_DIR)/$(GAME_NAME)-s1.bin
C1_ROM = $(BUILD_DIR)/$(GAME_NAME)-c1.bin
C2_ROM = $(BUILD_DIR)/$(GAME_NAME)-c2.bin
V1_ROM = $(BUILD_DIR)/$(GAME_NAME)-v1.bin
ELF_FILE = $(BUILD_DIR)/$(GAME_NAME).elf

# === Build Rules ===
.PHONY: all clean mame hal

all: hal $(P_ROM) $(M_ROM) $(S_ROM) $(C1_ROM) $(C2_ROM) $(V1_ROM)
	@echo ""
	@echo "Build complete!"
	@echo "ROM files:"
	@ls -la $(BUILD_DIR)/$(GAME_NAME)-*.bin

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# === Compile Sources ===
$(BUILD_DIR)/main.o: src/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# === Link ELF ===
$(ELF_FILE): $(OBJECTS) $(HAL_LIB) $(HAL_CRT0)
	$(CC) $(CFLAGS) $(HAL_LDFLAGS) $(HAL_CRT0) $(OBJECTS) $(HAL_LIB) -lgcc -o $@

# === ROM Generation ===

# P-ROM (68000 program, byte-swapped for MAME)
$(P_ROM): $(ELF_FILE)
	$(OBJCOPY) -O binary -S $< $@
	dd if=$@ of=$@ conv=notrunc,swab status=none
	truncate -s 128K $@ 2>/dev/null || dd if=/dev/null of=$@ bs=1 seek=131072 count=0

# M-ROM (Z80 audio driver)
$(M_ROM): $(HAL_Z80_DRIVER) | $(BUILD_DIR)
	$(Z80ASM) -o $(BUILD_DIR)/driver.rel $<
	sdld -n -i $(BUILD_DIR)/driver.ihx -b _CODE=0x0000 -b _DATA=0xF800 $(BUILD_DIR)/driver.rel
	sdobjcopy -I ihex -O binary $(BUILD_DIR)/driver.ihx $@
	truncate -s 64K $@ 2>/dev/null || dd if=/dev/null of=$@ bs=1 seek=65536 count=0
	@rm -f $(BUILD_DIR)/driver.rel $(BUILD_DIR)/driver.ihx $(BUILD_DIR)/driver.sym $(BUILD_DIR)/driver.map $(BUILD_DIR)/driver.noi

# S-ROM (fix layer graphics - use HAL's default sfix)
$(S_ROM): $(HAL_SFIX) | $(BUILD_DIR)
	cp $< $@

# C-ROM (sprite graphics - empty for this template)
$(C1_ROM): | $(BUILD_DIR)
	dd if=/dev/zero of=$@ bs=1K count=64 2>/dev/null

$(C2_ROM): | $(BUILD_DIR)
	dd if=/dev/zero of=$@ bs=1K count=64 2>/dev/null

# V-ROM (audio samples - empty for this template)
$(V1_ROM): | $(BUILD_DIR)
	dd if=/dev/zero of=$@ bs=1K count=512 2>/dev/null

# === MAME Target ===
mame: all $(BUILD_DIR)/$(MAME_DRIVER).zip
	@echo "Running in MAME..."
	mame $(MAME_DRIVER) -rompath "$(BUILD_DIR);$(NEOGEO_BIOS_PATH)" -nofilter -window -resolution 640x448 -skip_gameinfo

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

clean:
	rm -rf $(BUILD_DIR)
