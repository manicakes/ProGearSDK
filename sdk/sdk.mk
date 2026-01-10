# ProGearSDK Shared Makefile Include
#
# Include this file in your game's Makefile to use the SDK.
# Before including, define SDK_PATH to point to the sdk directory.
#
# Example:
#   SDK_PATH = ../../sdk
#   include $(SDK_PATH)/sdk.mk

# Verify SDK_PATH is set
ifndef SDK_PATH
$(error SDK_PATH must be defined before including sdk.mk)
endif

# === Toolchain ===
PREFIX ?= m68k-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
AR = $(PREFIX)ar
OBJCOPY = $(PREFIX)objcopy

# Z80 assembler (sdasz80 from SDCC)
Z80ASM = sdasz80

# === SDK Paths ===
SDK_INCLUDE = $(SDK_PATH)/include
SDK_LIB = $(SDK_PATH)/build/libprogearsdk.a
SDK_LINKER_SCRIPT = $(SDK_PATH)/rom/link.ld
SDK_SFIX = $(SDK_PATH)/rom/sfix.bin
SDK_Z80_DRIVER = $(SDK_PATH)/z80/driver.s
SDK_CRT0 = $(SDK_PATH)/build/crt0.o

# === Common Compiler Flags ===
SDK_CFLAGS = -m68000 -Os -fomit-frame-pointer -ffreestanding
SDK_CFLAGS += -Wall -Wextra -Wshadow -Wdouble-promotion -Wformat=2 -Wundef
SDK_CFLAGS += -fno-common -Wconversion -Wno-sign-conversion
SDK_CFLAGS += -I$(SDK_INCLUDE)

SDK_ASFLAGS = -m68000

SDK_LDFLAGS = -T$(SDK_LINKER_SCRIPT) -nostdlib

# === SDK Build Rule ===
# Call this target to ensure the SDK library is built
.PHONY: sdk
sdk:
	@$(MAKE) -C $(SDK_PATH) all
