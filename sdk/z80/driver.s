;;;
;;; This file is part of ProGearSDK.
;;; Copyright (c) 2024-2025 ProGearSDK contributors
;;; SPDX-License-Identifier: MIT
;;;

;;;
;;; NeoGeo DevKit Audio Driver
;;; Simple ADPCM-A (SFX) and ADPCM-B (Music) driver
;;;

    .module driver
    .area   CODE (ABS)

;;; === Constants ===

    ;; Z80 Stack location
    .equ    STACK_TOP,      0xFFFC

    ;; Z80 Ports
    .equ    PORT_FROM_68K,      0x00    ; Read command from 68k
    .equ    PORT_YM2610_A_ADDR, 0x04    ; YM2610 Port A address
    .equ    PORT_YM2610_A_VAL,  0x05    ; YM2610 Port A value
    .equ    PORT_YM2610_B_ADDR, 0x06    ; YM2610 Port B address
    .equ    PORT_YM2610_B_VAL,  0x07    ; YM2610 Port B value
    .equ    PORT_ENABLE_NMI,    0x08    ; Enable NMI from 68k
    .equ    PORT_TO_68K,        0x0C    ; Write reply to 68k
    .equ    PORT_DISABLE_NMI,   0x18    ; Disable NMI from 68k

    ;; YM2610 ADPCM-A Registers (Port B: 0x06/0x07)
    .equ    REG_ADPCM_A_CTRL,       0x00    ; Start/Stop control
    .equ    REG_ADPCM_A_MASTER_VOL, 0x01    ; Master volume (0-3F)
    .equ    REG_ADPCM_A1_PAN_VOL,   0x08    ; Ch1 pan + volume
    .equ    REG_ADPCM_A1_START_L,   0x10    ; Ch1 start addr LSB
    .equ    REG_ADPCM_A1_START_H,   0x18    ; Ch1 start addr MSB
    .equ    REG_ADPCM_A1_STOP_L,    0x20    ; Ch1 stop addr LSB
    .equ    REG_ADPCM_A1_STOP_H,    0x28    ; Ch1 stop addr MSB

    ;; YM2610 ADPCM-B Registers (Port A: 0x04/0x05)
    .equ    REG_ADPCM_B_CTRL,       0x10    ; Start/Stop/Repeat
    .equ    REG_ADPCM_B_PAN,        0x11    ; L/R output
    .equ    REG_ADPCM_B_START_L,    0x12    ; Start addr LSB
    .equ    REG_ADPCM_B_START_H,    0x13    ; Start addr MSB
    .equ    REG_ADPCM_B_STOP_L,     0x14    ; Stop addr LSB
    .equ    REG_ADPCM_B_STOP_H,     0x15    ; Stop addr MSB
    .equ    REG_ADPCM_B_DELTA_L,    0x19    ; Delta-N LSB
    .equ    REG_ADPCM_B_DELTA_H,    0x1A    ; Delta-N MSB
    .equ    REG_ADPCM_B_VOL,        0x1B    ; Volume (0-FF)
    .equ    REG_ADPCM_FLAG,         0x1C    ; Playback finished flags

    ;; Timer registers (Port A)
    .equ    REG_TIMER_FLAGS,        0x27    ; Timer control

    ;; Default Delta-N for ADPCM-B sample rates
    ;; Delta-N = (sample_rate / 55555) * 65536
    .equ    DELTA_N_44100,      0xCCCD  ; 44100 Hz
    .equ    DELTA_N_22050,      0x6666  ; 22050 Hz
    .equ    DELTA_N_18500,      0x5555  ; 18500 Hz (ADPCM-A native)
    .equ    DELTA_N_11025,      0x3333  ; 11025 Hz

;;; === Entry Point (0x0000) ===
    .org    0x0000
_start:
    di                          ; Disable interrupts
    jp      init_driver

;;; === RST handlers (unused) ===
    .org    0x0008
    ret
    .org    0x0010
    ret
    .org    0x0018
    ret
    .org    0x0020
    ret
    .org    0x0028
    ret
    .org    0x0030
    ret

;;; === INT handler (0x0038) - YM2610 timer interrupts ===
    .org    0x0038
int_handler:
    reti                        ; Not using timers yet

;;; === NMI handler (0x0066) - 68k command ===
    .org    0x0066
nmi_handler:
    push    af
    push    bc
    push    de
    push    hl

    ;; Read command from 68k
    in      a, (PORT_FROM_68K)
    ld      (current_cmd), a

    ;; Check for BIOS commands first
    cp      #0x01
    jp      z, cmd_slot_switch
    cp      #0x03
    jp      z, cmd_reset

    ;; Process regular commands
    call    process_command

    ;; Acknowledge command (echo with bit 7 set)
    ld      a, (current_cmd)
    or      #0x80
    out     (PORT_TO_68K), a

nmi_exit:
    pop     hl
    pop     de
    pop     bc
    pop     af
    retn

;;; === Driver ID (0x00C0) ===
    .org    0x00C0
    .ascii  "NGDEVKIT"

;;; === Main Code (0x0100) ===
    .org    0x0100

;;; Initialize the sound driver
init_driver:
    ld      sp, #STACK_TOP
    im      1                   ; Interrupt mode 1

    ;; Reset YM2610
    call    ym2610_reset

    ;; Initialize state
    xor     a
    ld      (adpcm_a_busy), a
    ld      (current_music), a
    ld      (next_channel), a
    ld      (music_paused), a
    ld      a, #0x3F
    ld      (master_volume), a

    ;; Enable NMI
    xor     a
    out     (PORT_ENABLE_NMI), a

    ;; Signal ready
    ld      a, #0x00
    out     (PORT_TO_68K), a

main_loop:
    ;; Check if ADPCM-B has ended and needs restart (for looping)
    call    check_music_loop

    ;; Small delay to avoid hammering the YM2610
    ld      b, #0x20
_delay_loop:
    djnz    _delay_loop

    jr      main_loop

;;; Check if music needs to loop (ADPCM-B ended)
check_music_loop:
    ;; Only check if music is playing and not paused
    ld      a, (current_music)
    or      a
    ret     z                   ; No music playing

    ld      a, (music_paused)
    or      a
    ret     nz                  ; Music is paused

    ;; Read ADPCM flag register to check if ADPCM-B ended (bit 7)
    ld      a, #REG_ADPCM_FLAG
    out     (PORT_YM2610_A_ADDR), a
    push    bc
    pop     bc                  ; Small delay
    in      a, (PORT_YM2610_A_VAL)

    bit     7, a                ; Check ADPCM-B end flag
    ret     z                   ; Not ended yet

    ;; Clear the end flag by writing 1 to bit 7
    push    bc
    ld      b, #REG_ADPCM_FLAG
    ld      a, #0x80
    call    ym_write_a
    pop     bc

    ;; Restart music from beginning
    ;; current_music stores index+1, so convert back to 0-31 for play_music
    ld      a, (current_music)
    dec     a
    jp      play_music

;;; === BIOS Commands ===

;;; Command 0x01: Prepare for slot switch
cmd_slot_switch:
    di
    ;; Mute everything
    call    ym2610_reset
    ;; Signal ready
    ld      a, #0x01
    out     (PORT_TO_68K), a
    ;; Wait in RAM
cmd_wait_ram:
    jr      cmd_wait_ram

;;; Command 0x03: Reset driver
cmd_reset:
    di
    jp      init_driver

;;; === Command Processing ===

;;; Process audio command in A
process_command:
    ld      a, (current_cmd)
    or      a
    ret     z                   ; 0x00 = NOP

    ;; 0x10-0x1F: Play SFX 0-15
    cp      #0x10
    jr      c, _check_music
    cp      #0x20
    jr      nc, _check_music
    sub     #0x10
    jp      play_sfx

_check_music:
    ;; 0x20-0x2F: Play music 0-15
    cp      #0x20
    jr      c, _check_stop_music
    cp      #0x30
    jr      nc, _check_stop_music
    sub     #0x20
    jp      play_music

_check_stop_music:
    ;; 0x30: Stop music
    cp      #0x30
    jr      nz, _check_pause
    jp      stop_music

_check_pause:
    ;; 0x31: Pause music
    cp      #0x31
    jr      nz, _check_resume
    jp      pause_music

_check_resume:
    ;; 0x32: Resume music
    cp      #0x32
    jr      nz, _check_sfx_ext
    jp      resume_music

_check_sfx_ext:
    ;; 0x40-0x4F: Play SFX 16-31
    cp      #0x40
    jr      c, _check_music_ext
    cp      #0x50
    jr      nc, _check_music_ext
    sub     #0x30               ; A = 16-31
    jp      play_sfx

_check_music_ext:
    ;; 0x50-0x5F: Play music 16-31
    cp      #0x50
    jr      c, _check_stop_sfx
    cp      #0x60
    jr      nc, _check_stop_sfx
    sub     #0x40               ; A = 16-31
    jp      play_music

_check_stop_sfx:
    ;; 0x60-0x65: Stop SFX channel 0-5
    cp      #0x60
    jr      c, _check_stop_all
    cp      #0x66
    jr      nc, _check_stop_all
    sub     #0x60
    jp      stop_sfx_channel

_check_stop_all:
    ;; 0x70: Stop all
    cp      #0x70
    jr      nz, _check_volume
    jp      stop_all

_check_volume:
    ;; 0x80-0x8F: Set master volume
    cp      #0x80
    jr      c, _check_pan_sfx
    cp      #0x90
    jr      nc, _check_pan_sfx
    and     #0x0F
    sla     a
    sla     a                   ; Volume 0-60 (in steps of 4)
    ld      (master_volume), a
    jp      set_master_volume

_check_pan_sfx:
    ;; 0xA0-0xAF: Play SFX with pan (SFX in low nibble)
    ;; Next command byte contains: pppp_vvvv (pan + volume)
    cp      #0xA0
    jr      c, _check_pan_music
    cp      #0xB0
    jr      nc, _check_pan_music
    and     #0x0F
    ld      (pending_sfx), a
    ;; Wait for next byte (pan/vol) - handled in extended mode
    ret

_check_pan_music:
    ;; 0xB0-0xBF: Play music with volume
    cp      #0xB0
    jr      c, _cmd_done
    cp      #0xC0
    jr      nc, _cmd_done
    and     #0x0F
    ld      (pending_music), a
    ret

_cmd_done:
    ret


;;; === ADPCM-A (Sound Effects) ===

;;; Play SFX sample A (0-31) on auto-assigned channel
;;; A = sample number (0-31)
play_sfx:
    push    bc
    push    de
    push    hl

    ;; Get next channel (round-robin)
    ld      d, a                ; D = sample number
    call    find_free_channel

    ;; A = channel (0-5), D = sample
    ld      e, a                ; E = channel
    ld      c, a                ; C = channel for later

    ;; Get sample info from table
    ;; Each entry: start_l, start_h, stop_l, stop_h (4 bytes)
    ld      h, #0
    ld      l, d
    add     hl, hl
    add     hl, hl              ; HL = sample * 4
    ld      de, #sfx_table
    add     hl, de              ; HL = &sfx_table[sample]

    ;; Stop channel first
    ld      a, c                ; Channel number
    ld      b, #1
_shift_channel:
    or      a
    jr      z, _got_channel_bit
    sla     b
    dec     a
    jr      _shift_channel
_got_channel_bit:
    ld      a, b
    or      #0x80               ; Stop bit
    ld      b, #REG_ADPCM_A_CTRL
    call    ym_write_b

    ;; Set start address LSB
    ld      a, #REG_ADPCM_A1_START_L
    add     a, c                ; Add channel offset
    ld      b, a
    ld      a, (hl)
    inc     hl
    call    ym_write_b

    ;; Set start address MSB
    ld      a, #REG_ADPCM_A1_START_H
    add     a, c
    ld      b, a
    ld      a, (hl)
    inc     hl
    call    ym_write_b

    ;; Set stop address LSB
    ld      a, #REG_ADPCM_A1_STOP_L
    add     a, c
    ld      b, a
    ld      a, (hl)
    inc     hl
    call    ym_write_b

    ;; Set stop address MSB
    ld      a, #REG_ADPCM_A1_STOP_H
    add     a, c
    ld      b, a
    ld      a, (hl)
    call    ym_write_b

    ;; Set pan + volume (centered, max volume)
    ld      a, #REG_ADPCM_A1_PAN_VOL
    add     a, c
    ld      b, a
    ld      a, #0xDF            ; L+R, volume 31
    call    ym_write_b

    ;; Start channel
    ld      a, c                ; Channel number
    ld      b, #1
_shift_start:
    or      a
    jr      z, _got_start_bit
    sla     b
    dec     a
    jr      _shift_start
_got_start_bit:
    ld      a, b                ; Channel bit (no stop)
    ld      b, #REG_ADPCM_A_CTRL
    call    ym_write_b

    ;; Mark channel as busy
    ld      a, (adpcm_a_busy)
    ld      d, a
    ld      a, c
    ld      b, #1
_shift_busy:
    or      a
    jr      z, _got_busy_bit
    sla     b
    dec     a
    jr      _shift_busy
_got_busy_bit:
    ld      a, d
    or      b
    ld      (adpcm_a_busy), a

_play_sfx_done:
    pop     hl
    pop     de
    pop     bc
    ret

;;; Find next ADPCM-A channel using round-robin
;;; Returns: A = channel (0-5)
find_free_channel:
    ld      a, (next_channel)
    ld      c, a                ; Save for return

    ;; Advance to next channel (0-5)
    inc     a
    cp      #6
    jr      c, _save_next
    xor     a                   ; Wrap to 0
_save_next:
    ld      (next_channel), a

    ld      a, c                ; Return current channel
    ret

;;; Stop SFX on channel A (0-5)
stop_sfx_channel:
    push    bc
    ld      c, a

    ;; Calculate channel bit
    ld      b, #1
_stop_shift:
    or      a
    jr      z, _stop_got_bit
    sla     b
    dec     a
    jr      _stop_shift
_stop_got_bit:

    ;; Stop channel (set bit + 0x80)
    ld      a, b
    or      #0x80
    ld      b, #REG_ADPCM_A_CTRL
    call    ym_write_b

    ;; Clear busy flag
    ld      a, c
    ld      b, #1
_clear_shift:
    or      a
    jr      z, _clear_got_bit
    sla     b
    dec     a
    jr      _clear_shift
_clear_got_bit:
    ld      a, b
    cpl
    ld      b, a
    ld      a, (adpcm_a_busy)
    and     b
    ld      (adpcm_a_busy), a

    pop     bc
    ret


;;; === ADPCM-B (Music) ===

;;; Play music sample A (0-31) with looping
;;; A = sample number
play_music:
    push    bc
    push    de
    push    hl

    ;; Store music index + 1 (so 0 = no music, 1-32 = music 0-31)
    ld      c, a                ; Save music index
    inc     a
    ld      (current_music), a
    xor     a
    ld      (music_paused), a   ; Clear paused state when starting new music
    ld      a, c                ; Restore music index (0-31) for table lookup

    ;; Get music info from table
    ;; Each entry: start_l, start_h, stop_l, stop_h, delta_l, delta_h (6 bytes)
    ld      h, #0
    ld      l, a
    add     hl, hl              ; * 2
    ld      d, h
    ld      e, l
    add     hl, hl              ; * 4
    add     hl, de              ; * 6
    ld      de, #music_table
    add     hl, de

    ;; Stop current playback
    ld      b, #REG_ADPCM_B_CTRL
    ld      a, #0x01            ; Reset
    call    ym_write_a

    ;; Set start address
    ld      b, #REG_ADPCM_B_START_L
    ld      a, (hl)
    inc     hl
    call    ym_write_a

    ld      b, #REG_ADPCM_B_START_H
    ld      a, (hl)
    inc     hl
    call    ym_write_a

    ;; Set stop address
    ld      b, #REG_ADPCM_B_STOP_L
    ld      a, (hl)
    inc     hl
    call    ym_write_a

    ld      b, #REG_ADPCM_B_STOP_H
    ld      a, (hl)
    inc     hl
    call    ym_write_a

    ;; Set delta-N (sample rate)
    ld      b, #REG_ADPCM_B_DELTA_L
    ld      a, (hl)
    inc     hl
    call    ym_write_a

    ld      b, #REG_ADPCM_B_DELTA_H
    ld      a, (hl)
    call    ym_write_a

    ;; Set pan (center)
    ld      b, #REG_ADPCM_B_PAN
    ld      a, #0xC0            ; L+R
    call    ym_write_a

    ;; Set volume
    ld      b, #REG_ADPCM_B_VOL
    ld      a, #0xFF            ; Max volume
    call    ym_write_a

    ;; Start with repeat
    ld      b, #REG_ADPCM_B_CTRL
    ld      a, #0x90            ; Start + Repeat
    call    ym_write_a

    pop     hl
    pop     de
    pop     bc
    ret

;;; Stop music playback
stop_music:
    push    bc

    ;; Reset ADPCM-B
    ld      b, #REG_ADPCM_B_CTRL
    ld      a, #0x01            ; Reset
    call    ym_write_a

    xor     a
    ld      (current_music), a
    ld      (music_paused), a

    pop     bc
    ret

;;; Pause music playback
pause_music:
    push    bc

    ;; Clear start bit to pause (keep repeat bit)
    ld      b, #REG_ADPCM_B_CTRL
    ld      a, #0x00            ; Stop without reset
    call    ym_write_a

    ld      a, #1
    ld      (music_paused), a

    pop     bc
    ret

;;; Resume music playback
resume_music:
    push    bc

    ;; Check if actually paused
    ld      a, (music_paused)
    or      a
    jr      z, _resume_done

    ;; Set start + repeat bits to resume
    ld      b, #REG_ADPCM_B_CTRL
    ld      a, #0x90            ; Start + Repeat
    call    ym_write_a

    xor     a
    ld      (music_paused), a

_resume_done:
    pop     bc
    ret

;;; Stop all audio
stop_all:
    push    bc

    ;; Stop all ADPCM-A channels
    ld      b, #REG_ADPCM_A_CTRL
    ld      a, #0xBF            ; Stop all 6 channels
    call    ym_write_b

    ;; Stop ADPCM-B
    ld      b, #REG_ADPCM_B_CTRL
    ld      a, #0x01
    call    ym_write_a

    xor     a
    ld      (adpcm_a_busy), a
    ld      (current_music), a

    pop     bc
    ret

;;; Set master volume (A = volume 0-63)
set_master_volume:
    push    bc
    ld      b, #REG_ADPCM_A_MASTER_VOL
    call    ym_write_b
    pop     bc
    ret


;;; === YM2610 Low-Level Access ===

;;; Reset YM2610
ym2610_reset:
    push    bc

    ;; Reset timers
    ld      b, #REG_TIMER_FLAGS
    ld      a, #0x30
    call    ym_write_a

    ;; Stop all ADPCM-A
    ld      b, #REG_ADPCM_A_CTRL
    ld      a, #0xBF
    call    ym_write_b

    ;; Set ADPCM-A master volume
    ld      b, #REG_ADPCM_A_MASTER_VOL
    ld      a, #0x3F
    call    ym_write_b

    ;; Stop ADPCM-B
    ld      b, #REG_ADPCM_B_CTRL
    ld      a, #0x01
    call    ym_write_a

    ;; Clear playback flags
    ld      b, #REG_ADPCM_FLAG
    ld      a, #0xBF
    call    ym_write_a

    pop     bc
    ret

;;; Write to YM2610 Port A
;;; B = register, A = value
ym_write_a:
    push    af
    ld      a, b
    out     (PORT_YM2610_A_ADDR), a
    call    ym_wait_addr
    pop     af
    out     (PORT_YM2610_A_VAL), a
    call    ym_wait_data
    ret

;;; Write to YM2610 Port B
;;; B = register, A = value
ym_write_b:
    push    af
    ld      a, b
    out     (PORT_YM2610_B_ADDR), a
    call    ym_wait_addr
    pop     af
    out     (PORT_YM2610_B_VAL), a
    call    ym_wait_data
    ret

;;; Wait for YM2610 address write (min 2.125µs)
ym_wait_addr:
    nop
    ret

;;; Wait for YM2610 data write (min 10.375µs)
ym_wait_data:
    push    bc
    pop     bc
    push    bc
    pop     bc
    push    bc
    pop     bc
    ret


;;; === Sample Tables (fixed at 0x0800) ===
;;; ngres will generate these at the known offset
;;; The Makefile patches these after building the driver

    .org    0x0800

;;; SFX table: 32 entries, 4 bytes each (start_l, start_h, stop_l, stop_h)
;;; Addresses are in 256-byte units
sfx_table::
    .ds     128                 ; 32 * 4 bytes

;;; Music table: 32 entries, 6 bytes each (start_l, start_h, stop_l, stop_h, delta_l, delta_h)
music_table::
    .ds     192                 ; 32 * 6 bytes


;;; === Data Section ===
;;; Located in Z80 RAM (0xF800-0xFFFF)
    .area   _DATA

current_cmd:
    .ds     1

adpcm_a_busy:
    .ds     1                   ; Bit flags for busy channels

current_music:
    .ds     1                   ; Currently playing music index

master_volume:
    .ds     1

pending_sfx:
    .ds     1

pending_music:
    .ds     1

next_channel:
    .ds     1                   ; Round-robin channel counter (0-5)

music_paused:
    .ds     1                   ; 1 if music is paused
