# NeoGeo HAL Shared Makefile Include
#
# Include this file in your application's Makefile to use the HAL directly
# (without the SDK game engine). This provides everything needed to build
# a complete NeoGeo application.
#
# Before including, define HAL_PATH to point to the hal directory.
#
# Example:
#   HAL_PATH = ../../hal
#   include $(HAL_PATH)/hal.mk

# Verify HAL_PATH is set
ifndef HAL_PATH
$(error HAL_PATH must be defined before including hal.mk)
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
HAL_INCLUDE = $(HAL_PATH)/include
HAL_LIB = $(HAL_PATH)/build/libneogeo.a
HAL_CRT0 = $(HAL_PATH)/build/crt0.o
HAL_LINKER_SCRIPT = $(HAL_PATH)/rom/link.ld
HAL_SFIX = $(HAL_PATH)/rom/sfix.bin
HAL_Z80_DRIVER = $(HAL_PATH)/z80/driver.s

# === Common Compiler Flags ===
HAL_CFLAGS = -m68000 -Os -fomit-frame-pointer -ffreestanding
HAL_CFLAGS += -Wall -Wextra -Wshadow -Wdouble-promotion -Wformat=2 -Wundef
HAL_CFLAGS += -fno-common -Wconversion -Wno-sign-conversion
HAL_CFLAGS += -I$(HAL_INCLUDE)

HAL_ASFLAGS = -m68000

HAL_LDFLAGS = -T$(HAL_LINKER_SCRIPT) -nostdlib

# === HAL Build Rule ===
.PHONY: hal
hal:
	@$(MAKE) -C $(HAL_PATH) all
