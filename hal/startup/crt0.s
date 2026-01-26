| This file is part of ProGearSDK.
| Copyright (c) 2024-2025 ProGearSDK contributors
| SPDX-License-Identifier: MIT

| crt0.s - NeoGeo 68000 startup code
| Minimal header based on wiki.neogeodev.org/index.php?title=68k_program_header

    .text
    .global _start
    .global NGWaitVBlank

| External interrupt handler pointers (defined in ng_interrupt.c)
    .extern ng_vblank_handler
    .extern ng_timer_handler

| Data section bounds (defined in link.ld)
    .extern __data_start
    .extern __data_end
    .extern __data_load

| BSS section bounds (defined in link.ld)
    .extern __bss_start
    .extern __bss_end

| ============================================================================
| 68000 Exception Vector Table (0x000000 - 0x0000FF)
| ============================================================================
    .org    0x000000

_vectors:
    .long   0x0010F300          | 00: Initial SP
    .long   0x00C00402          | 04: Initial PC -> BIOS init
    .long   0x00C00408          | 08: Bus error -> BIOS
    .long   0x00C0040E          | 0C: Address error -> BIOS
    .long   0x00C00414          | 10: Illegal instruction -> BIOS
    .long   0x00C0041A          | 14: Division by zero -> BIOS
    .long   0x00C0041A          | 18: CHK instruction -> BIOS
    .long   0x00C0041A          | 1C: TRAPV instruction -> BIOS
    .long   0x00C0041A          | 20: Privilege violation -> BIOS
    .long   0x00C00420          | 24: Trace -> BIOS
    .long   0x00C00426          | 28: Line 1010 emulator -> BIOS
    .long   0x00C00426          | 2C: Line 1111 emulator -> BIOS
    .long   0xFFFFFFFF          | 30: Reserved
    .long   0xFFFFFFFF          | 34: Reserved
    .long   0xFFFFFFFF          | 38: Reserved
    .long   0x00C0042C          | 3C: Uninitialized interrupt -> BIOS
    .long   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF  | 40-4C
    .long   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF  | 50-5C
    .long   0x00C00432          | 60: Spurious interrupt -> BIOS
    .long   _vblank             | 64: Level 1 interrupt (VBlank)
    .long   _timer              | 68: Level 2 interrupt (Timer)
    .long   0x00000000          | 6C: Level 3 interrupt (unused)
    .long   0x00000000, 0x00000000, 0x00000000, 0x00000000  | 70-7C
    | 0x80-0xFF: Unused vectors (fill with zeros like ngdevkit)
    .rept   32
    .long   0x00000000
    .endr

| ============================================================================
| NeoGeo ROM Header
| ============================================================================
    .org    0x000100

    .ascii  "NEO-GEO"           | 0x100: Magic string
    .byte   0x00                | 0x107: Null terminator

    .org    0x000108
    .word   0x0539              | 0x108: NGH number (must be > 0)
    .word   0x0010              | 0x10A: Program bank size (16 = 1MB)
    .long   0x00100000          | 0x10C: Program ROM size
    .long   0x00000000          | 0x110: Reserved

    .org    0x000114
    | Eyecatcher config: dd swab swaps these bytes, then MAME unswaps on load,
    | so BIOS sees exactly what we write here.
    .byte   0x00                | Eye catcher animation (0=cart, 1=BIOS, 2=skip)
    .byte   0x01                | Eye catcher sprite bank (bank 1 = tiles 256-319)
    .long   _ings_name          | 0x116: Pointer to JP soft DIPs
    .long   _ings_name          | 0x11A: Pointer to US soft DIPs
    .long   _ings_name          | 0x11E: Pointer to EU soft DIPs

    .org    0x000122
    jmp     _user_init.l        | 0x122: USER entry point
    jmp     _dummy_rts.l        | 0x128: PLAYER_START
    jmp     _dummy_rts.l        | 0x12E: DEMO
    jmp     _dummy_rts.l        | 0x134: COIN_SOUND

| ============================================================================
| Security Code Pointer and Code (from wiki minimal example)
| ============================================================================
    .org    0x000182

    .long   _scode              | 0x182: Pointer to security code

_scode:
    .long   0x76004A6D, 0x0A146600, 0x003C206D, 0x0A043E2D
    .long   0x0A0813C0, 0x00300001, 0x32100C01, 0x00FF671A
    .long   0x30280002, 0xB02D0ACE, 0x66103028, 0x0004B02D
    .long   0x0ACF6606, 0xB22D0AD0, 0x67085088, 0x51CFFFD4
    .long   0x36074E75, 0x206D0A04, 0x3E2D0A08, 0x3210E049
    .long   0x0C0100FF, 0x671A3010, 0xB02D0ACE, 0x66123028
    .long   0x0002E048, 0xB02D0ACF, 0x6606B22D, 0x0AD06708
    .long   0x588851CF, 0xFFD83607
    .word   0x4E75

| ============================================================================
| USER_INIT - Called by BIOS at startup
| Just call main() directly - it will loop forever
| ============================================================================
_user_init:
    move.b  %d0, 0x300001       | Kick watchdog

    | Copy initialized data from ROM to RAM
    lea     __data_load, %a0    | Source (ROM)
    lea     __data_start, %a1   | Destination (RAM)
    lea     __data_end, %a2     | End marker
    cmp.l   %a1, %a2
    beq.s   1f                  | Skip if .data is empty
0:  move.l  (%a0)+, (%a1)+
    cmp.l   %a1, %a2
    bhi.s   0b
1:
    | Clear BSS section (zero-initialized data)
    lea     __bss_start, %a0
    lea     __bss_end, %a1
    cmp.l   %a0, %a1
    beq.s   3f                  | Skip if BSS is empty
2:  clr.l   (%a0)+
    cmp.l   %a0, %a1
    bhi.s   2b
3:
    bset    #7, 0x10FD80        | Set bit 7 of BIOS_SYSTEM_MODE (game has control)
    move.w  #0x2000, %sr        | Enable interrupts
    jmp     main                | Jump to main (never returns)

| ============================================================================
| VBlank Interrupt Handler
| Must check BIOS system mode and call BIOS VBlank processing
| ============================================================================
_vblank:
    movem.l %d0-%d1/%a0-%a1, -(%sp)  | Save registers
    btst    #7, 0x10FD80        | Check bit 7 of BIOS_SYSTEM_MODE
    bne.s   1f                  | If set, handle it ourselves
    movem.l (%sp)+, %d0-%d1/%a0-%a1  | Restore registers
    jmp     0xC00438.l          | Let BIOS handle VBlank
1:
    move.w  #4, 0x3C000C        | Acknowledge VBlank interrupt
    move.b  %d0, 0x300001       | Kick watchdog
    move.b  #1, 0x10FD8E        | Set vblank flag for NG_waitVBlank
    | Check for custom VBlank handler
    move.l  ng_vblank_handler, %a0
    cmp.l   #0, %a0
    beq.s   2f                  | No custom handler
    jsr     (%a0)               | Call custom handler
2:
    movem.l (%sp)+, %d0-%d1/%a0-%a1  | Restore registers
    rte

| ============================================================================
| Timer Interrupt Handler
| Used for raster effects (mid-frame register changes)
| ============================================================================
_timer:
    movem.l %d0-%d1/%a0-%a1, -(%sp)  | Save registers
    move.w  #2, 0x3C000C        | Acknowledge timer interrupt
    move.b  %d0, 0x300001       | Kick watchdog (prevent reset)
    | Check for custom Timer handler
    move.l  ng_timer_handler, %a0
    cmp.l   #0, %a0
    beq.s   1f                  | No custom handler
    jsr     (%a0)               | Call custom handler
1:
    movem.l (%sp)+, %d0-%d1/%a0-%a1  | Restore registers
    rte

| ============================================================================
| NGWaitVBlank - Wait for vertical blank
| ============================================================================
NGWaitVBlank:
    move.b  %d0, 0x300001       | Kick watchdog
1:  tst.b   0x10FD8E            | Check vblank flag
    beq.s   1b
    clr.b   0x10FD8E            | Clear flag
    rts

| ============================================================================
| Dummy handler for unused BIOS callbacks - must return via BIOS
| ============================================================================
_dummy_rts:
    jmp     0xC00444.l          | Return to BIOS system

| ============================================================================
| Startup loop (if reset hits here, just kick watchdog)
| ============================================================================
_start:
    move.b  %d0, 0x300001
    bra.s   _start

| ============================================================================
| ROM name for soft DIPs (16 bytes + padding)
| ============================================================================
_ings_name:
    .ascii  "PROGEARSDK      "  | 16 chars ROM name
    .long   0xFFFFFFFF          | Padding
    .long   0xFFFFFFFF
    .long   0x00000000
    .long   0x00000000
    .long   0x00000000
    .long   0x00000000

    .end
