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

# === HAL Paths ===
HAL_PATH = $(SDK_PATH)/../hal
HAL_INCLUDE = $(HAL_PATH)/include
HAL_LIB = $(HAL_PATH)/build/libneogeo.a
HAL_CRT0 = $(HAL_PATH)/build/crt0.o
HAL_LINKER_SCRIPT = $(HAL_PATH)/rom/link.ld
HAL_SFIX = $(HAL_PATH)/rom/sfix.bin
HAL_Z80_DRIVER = $(HAL_PATH)/z80/driver.s

# === SDK Paths ===
SDK_INCLUDE = $(SDK_PATH)/include
SDK_LIB = $(SDK_PATH)/build/libprogearsdk.a

# === Aliases for backward compatibility ===
SDK_CRT0 = $(HAL_CRT0)
SDK_LINKER_SCRIPT = $(HAL_LINKER_SCRIPT)
SDK_SFIX = $(HAL_SFIX)
SDK_Z80_DRIVER = $(HAL_Z80_DRIVER)

# === Common Compiler Flags ===
SDK_CFLAGS = -m68000 -Os -fomit-frame-pointer -ffreestanding
SDK_CFLAGS += -Wall -Wextra -Wshadow -Wdouble-promotion -Wformat=2 -Wundef
SDK_CFLAGS += -fno-common -Wconversion -Wno-sign-conversion
SDK_CFLAGS += -I$(SDK_INCLUDE) -I$(HAL_INCLUDE)

SDK_ASFLAGS = -m68000

SDK_LDFLAGS = -T$(SDK_LINKER_SCRIPT) -nostdlib

# === Libraries for linking (order matters: SDK first, then HAL) ===
SDK_LIBS = $(SDK_LIB) $(HAL_LIB)

# === HAL Build Rule ===
.PHONY: hal
hal:
	@$(MAKE) -C $(HAL_PATH) all

# === SDK Build Rule ===
# Call this target to ensure both HAL and SDK libraries are built
.PHONY: sdk
sdk: hal
	@$(MAKE) -C $(SDK_PATH) all
