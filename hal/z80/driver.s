;;;
;;; This file is part of ProGearSDK.
;;; Copyright (c) 2024-2025 ProGearSDK contributors
;;; SPDX-License-Identifier: MIT
;;;

;;;
;;; NeoGeo ProGearSDK Audio Driver
;;; Simple ADPCM-A (SFX) and ADPCM-B (Music) driver
;;;
;;; 68k/Z80 Communication Protocol:
;;; - 68k writes commands to REG_SOUND, which triggers Z80 NMI
;;; - Z80 reads commands from port $00, replies via port $0C
;;; - Standard commands acknowledged by echoing with bit 7 set (cmd | 0x80)
;;;
;;; Mandatory BIOS Commands (must be implemented per SNK spec):
;;; - $01: Slot switch - stop sounds, enable NMI, reply $01, wait in RAM
;;;        Failure causes "Z80 ERROR" during BIOS self-test
;;; - $02: Eyecatcher - play boot music (no reply)
;;; - $03: Reset - soft reset within 100ms (no reply)
;;;
;;; See: https://wiki.neogeodev.org/index.php?title=68k/Z80_communication
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
    .equ    REG_TIMER_B_LOAD,       0x26    ; Timer B load value
    .equ    REG_TIMER_FLAGS,        0x27    ; Timer control

    ;; FM registers
    .equ    REG_FM_LFO,             0x22    ; LFO enable/rate (Port A)
    .equ    REG_FM_KEYON,           0x28    ; Key on/off (Port A, all channels)
    .equ    REG_FM_FNUM_LO,         0xA0    ; F-number LSB (+channel offset)
    .equ    REG_FM_FNUM_HI,         0xA4    ; Block + F-number MSB (write first!)
    .equ    REG_FM_FB_ALGO,         0xB0    ; Feedback/algorithm
    .equ    REG_FM_LR_AMS_PMS,      0xB4    ; Stereo output + LFO sensitivity

    ;; Boot music tempo: ~136 BPM (traced from pbobblen/shocktro, which
    ;; both play the jingle with a 55.3ms 1/32-note grid).
    ;; Timer B period is 1152 * (256 - value) / 4MHz = 288us per count:
    ;; (256 - 64) * 288us = 55.30ms
    .equ    TIMER_B_32ND,   64

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

    ;; Check for BIOS commands first (0x01, 0x02, 0x03)
    cp      #0x01
    jp      z, cmd_slot_switch
    cp      #0x02
    jp      z, cmd_eyecatcher
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
    .ascii  "PROGEARSDK"

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
    ld      (pending_pan), a
    ld      (fm_active), a
    ld      (fm_request), a
    ld      (seq_active), a
    ld      (seq_ntracks), a
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

    ;; Boot music: start on request, then advance on each Timer B expiry
    ld      a, (fm_request)
    or      a
    call    nz, fm_boot_start
    ld      a, (fm_active)
    or      a
    call    nz, fm_poll

    ;; Sequenced song: advance on each Timer B expiry
    ld      a, (seq_active)
    or      a
    call    nz, seq_poll

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
;;; Per SNK spec: stop sounds, enable NMI, send $01 back, then wait in RAM loop
;;; Failure to reply triggers "Z80 ERROR" during BIOS self-test
cmd_slot_switch:
    di
    ;; Mute everything
    call    ym2610_reset

    ;; Copy wait loop to RAM and jump there
    ;; This ensures the Z80 survives any ROM switching
    ld      hl, #slot_wait_code
    ld      de, #SLOT_WAIT_RAM
    ld      bc, #slot_wait_end - slot_wait_code
    ldir

    ;; Enable NMI before signaling ready (per SNK spec)
    xor     a
    out     (PORT_ENABLE_NMI), a

    ;; Signal ready to 68k
    ld      a, #0x01
    out     (PORT_TO_68K), a

    ;; Jump to wait loop in RAM
    jp      SLOT_WAIT_RAM

;;; Wait loop code to be copied to RAM
slot_wait_code:
    jr      slot_wait_code
slot_wait_end:

;;; RAM address for slot switch wait loop
    .equ    SLOT_WAIT_RAM,  0xFFF0

;;; Command 0x02: Play eyecatcher music
;;; Used by BIOS to request boot music on cartridge systems
;;; No acknowledgment expected
cmd_eyecatcher:
    ;; Stop any ADPCM-B music, then request the FM boot jingle.
    ;; Playback starts from the main loop so the NMI handler stays short
    ;; and YM writes are not interleaved with a game command's writes.
    call    stop_music
    ld      a, #1
    ld      (fm_request), a
    ;; No reply per SNK spec
    jp      nmi_exit

;;; Command 0x03: Reset driver
;;; Must complete within 100ms per SNK spec
;;; No acknowledgment expected
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
    jr      c, _check_seq_play
    cp      #0x90
    jr      nc, _check_seq_play
    and     #0x0F
    sla     a
    sla     a                   ; Volume 0-60 (in steps of 4)
    ld      (master_volume), a
    jp      set_master_volume

_check_seq_play:
    ;; 0xA0-0xAF: Play sequenced (FM) song 0-15
    cp      #0xA0
    jr      c, _check_seq_stop
    cp      #0xB0
    jr      nc, _check_seq_stop
    and     #0x0F
    jp      seq_start

_check_seq_stop:
    ;; 0xB0: Stop sequenced song
    cp      #0xB0
    jr      nz, _check_sfx_left
    jp      seq_stop

_check_sfx_left:
    ;; 0xC0-0xCF: Play SFX 0-15 panned LEFT
    cp      #0xC0
    jr      c, _check_sfx_right
    cp      #0xD0
    jr      nc, _check_sfx_right
    push    af
    ld      a, #0x80            ; Left pan
    ld      (pending_pan), a
    pop     af
    and     #0x0F
    jp      play_sfx

_check_sfx_right:
    ;; 0xD0-0xDF: Play SFX 0-15 panned RIGHT
    cp      #0xD0
    jr      c, _check_sfx_ext_left
    cp      #0xE0
    jr      nc, _check_sfx_ext_left
    push    af
    ld      a, #0x40            ; Right pan
    ld      (pending_pan), a
    pop     af
    and     #0x0F
    jp      play_sfx

_check_sfx_ext_left:
    ;; 0xE0-0xEF: Play SFX 16-31 panned LEFT
    cp      #0xE0
    jr      c, _check_sfx_ext_right
    cp      #0xF0
    jr      nc, _check_sfx_ext_right
    push    af
    ld      a, #0x80            ; Left pan
    ld      (pending_pan), a
    pop     af
    sub     #0xD0               ; A = 16-31
    jp      play_sfx

_check_sfx_ext_right:
    ;; 0xF0-0xFF: Play SFX 16-31 panned RIGHT
    cp      #0xF0
    jr      c, _cmd_done
    push    af
    ld      a, #0x40            ; Right pan
    ld      (pending_pan), a
    pop     af
    sub     #0xE0               ; A = 16-31
    jp      play_sfx

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

    ;; Set pan + volume from pending_pan (or default center)
    ld      a, #REG_ADPCM_A1_PAN_VOL
    add     a, c
    ld      b, a
    ld      a, (pending_pan)
    or      a
    jr      nz, _use_pending_pan
    ld      a, #0xDF            ; L+R, volume 31 (default center)
    jr      _set_pan_vol
_use_pending_pan:
    or      #0x1F               ; Add max volume (0-31) to pan bits
_set_pan_vol:
    call    ym_write_b
    xor     a
    ld      (pending_pan), a    ; Clear pending pan

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

    ;; Stop FM boot music if playing
    call    fm_stop
    xor     a
    ld      (fm_request), a

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


;;; === FM Boot Music (eyecatcher jingle) ===
;;;
;;; Faithful reproduction of the arrangement SNK games play, reverse
;;; engineered from YM2610 register traces of pbobblen and shocktro
;;; (both play byte-identical note/velocity/pan data):
;;; - FM1: jingle melody          FM2: its echo (+1 tick, fnum-6, +2 TL)
;;; - FM3: 1/32-note run line     FM4: its echo (+1 tick, fnum-6, +2 TL)
;;; The run line starts 5 ticks before the melody, doubles every pitch
;;; (strong then soft), and auto-pans C/R/C/L every 4 notes. Velocity
;;; is a per-event 4-bit value scaled to carrier TL.
;;; Sequencer is driven by YM2610 Timer B, polled from the main loop.
;;;
;;; FM channel addressing (see wiki: FM, YM2610 registers):
;;; - Channels 1/2: registers on port A at base+1/base+2, key-on codes 1/2
;;; - Channels 3/4: registers on port B at base+1/base+2, key-on codes 5/6
;;; - Key-on register 0x28 is always written through port A
;;; - Block/F-num high (0xA4+ch) must be written before F-num low (0xA0+ch)

;;; Track state layout (fm_tracks, 12 bytes per track):
    .equ    TRK_ACTIVE,     0   ; 1 while the track is playing
    .equ    TRK_PTR_L,      1   ; stream read pointer
    .equ    TRK_PTR_H,      2
    .equ    TRK_DUR,        3   ; 1/32s left of current event
    .equ    TRK_WAIT,       4   ; 1/32s to wait before starting (echo delay)
    .equ    TRK_FNUM_SUB,   5   ; subtracted from F-number (echo detune)
    .equ    TRK_KEYON,      6   ; key-on channel code (1/2/5/6)
    .equ    TRK_CHOFF,      7   ; register offset (1 or 2)
    .equ    TRK_PORT,       8   ; 0 = port A, 1 = port B
    .equ    TRK_TL,         9   ; carrier base level (velocity 0)
    .equ    TRK_FLAGS,      10  ; bit 0: auto-pan cycle
    .equ    TRK_CNT,        11  ; key-on counter for the pan cycle
    .equ    TRK_SIZE,       12

;;; Begin boot music playback (main loop context, on BIOS command 0x02)
fm_boot_start:
    xor     a
    ld      (fm_request), a

    ;; Keep 68k commands out while we batch ~100 YM writes
    out     (PORT_DISABLE_NMI), a

    ;; LFO off (the original patches use none)
    ld      b, #REG_FM_LFO
    xor     a
    call    ym_write_a

    ;; Melody patch on channels 1/2, run patch on channels 3/4
    ;; (echo loudness is per-track carrier TL, written per note)
    ld      hl, #fm_patch_melody
    ld      c, #1               ; channel offset 1
    xor     a                   ; port A
    call    fm_load_patch
    ld      hl, #fm_patch_melody
    ld      c, #2
    xor     a
    call    fm_load_patch
    ld      hl, #fm_patch_run
    ld      c, #1
    ld      a, #1               ; port B
    call    fm_load_patch
    ld      hl, #fm_patch_run
    ld      c, #2
    ld      a, #1
    call    fm_load_patch

    ;; Silence all 4 channels (slots off)
    call    fm_keyoff_all

    ;; Reset track states from the ROM template
    ld      hl, #fm_track_init
    ld      de, #fm_tracks
    ld      bc, #4 * TRK_SIZE
    ldir

    ;; Start Timer B: clear stale flags, enable + load B
    ld      b, #REG_TIMER_B_LOAD
    ld      a, #TIMER_B_32ND
    call    ym_write_a
    ld      b, #REG_TIMER_FLAGS
    ld      a, #0x3A            ; Reset A+B flags | Enable B | Load B
    call    ym_write_a

    ld      a, #1
    ld      (fm_active), a

    out     (PORT_ENABLE_NMI), a
    ret

;;; Poll Timer B and advance the sequencer on expiry (main loop context)
fm_poll:
    in      a, (PORT_YM2610_A_ADDR)     ; YM2610 status 0
    bit     1, a                        ; Timer B flag
    ret     z

    ;; Guard YM writes against NMI handler writes
    out     (PORT_DISABLE_NMI), a

    ;; Acknowledge: reset B flag, keep enable + load
    ld      b, #REG_TIMER_FLAGS
    ld      a, #0x2A
    call    ym_write_a

    ld      ix, #fm_tracks
    call    fm_track_tick
    ld      ix, #fm_tracks + TRK_SIZE
    call    fm_track_tick
    ld      ix, #fm_tracks + 2*TRK_SIZE
    call    fm_track_tick
    ld      ix, #fm_tracks + 3*TRK_SIZE
    call    fm_track_tick

    ;; All tracks finished?
    ld      a, (fm_tracks + TRK_ACTIVE)
    ld      hl, #fm_tracks + TRK_SIZE + TRK_ACTIVE
    or      (hl)
    ld      hl, #fm_tracks + 2*TRK_SIZE + TRK_ACTIVE
    or      (hl)
    ld      hl, #fm_tracks + 3*TRK_SIZE + TRK_ACTIVE
    or      (hl)
    jr      nz, _fm_poll_done

    ;; Song over: stop Timer B, mark idle
    call    fm_stop
_fm_poll_done:
    out     (PORT_ENABLE_NMI), a
    ret

;;; Stop boot music: key off channels, stop Timer B, clear state
fm_stop:
    call    fm_keyoff_all
    ld      b, #REG_TIMER_FLAGS
    ld      a, #0x30            ; Reset flags, timers stopped
    call    ym_write_a
    xor     a
    ld      (fm_active), a
    ret

;;; Key off all 4 FM channels
fm_keyoff_all:
    push    bc
    ld      b, #REG_FM_KEYON
    ld      a, #0x01
    call    ym_write_a
    ld      b, #REG_FM_KEYON
    ld      a, #0x02
    call    ym_write_a
    ld      b, #REG_FM_KEYON
    ld      a, #0x05
    call    ym_write_a
    ld      b, #REG_FM_KEYON
    ld      a, #0x06
    call    ym_write_a
    pop     bc
    ret

;;; Load a 30-register instrument patch into one FM channel
;;; HL = value table, C = channel register offset (1/2), A = port (0=A, 1=B)
fm_load_patch:
    ld      (fm_cur_port), a
    ld      de, #fm_patch_regs
    ld      b, #30
_patch_loop:
    push    bc
    ld      a, (de)             ; register base
    inc     de
    add     a, c                ; + channel offset
    ld      b, a
    ld      a, (hl)             ; value
    inc     hl
    call    fm_write_cur
    pop     bc
    djnz    _patch_loop
    ret

;;; Write B=register, A=value to the port selected by fm_cur_port
fm_write_cur:
    push    af
    ld      a, (fm_cur_port)
    or      a
    jr      nz, _fwc_port_b
    pop     af
    jp      ym_write_a
_fwc_port_b:
    pop     af
    jp      ym_write_b

;;; Advance one track by one 1/32 note (IX = track state)
fm_track_tick:
    ld      a, TRK_ACTIVE (ix)
    or      a
    ret     z

    ;; Echo delay before the track starts
    ld      a, TRK_WAIT (ix)
    or      a
    jr      z, _trk_run
    dec     TRK_WAIT (ix)
    ret

_trk_run:
    ld      a, TRK_DUR (ix)
    or      a
    jr      z, _trk_fetch       ; current event exhausted
    dec     TRK_DUR (ix)
    jr      z, _trk_fetch       ; fetch as soon as it hits 0: notes play
    ret                         ; legato, retriggering articulates repeats

_trk_fetch:
    ld      l, TRK_PTR_L (ix)
    ld      h, TRK_PTR_H (ix)
    ld      a, (hl)
    cp      #0xFF
    jp      z, _trk_end
    inc     hl
    ld      d, a                ; D = velocity<<4 | note id
    ld      a, (hl)
    inc     hl
    ld      TRK_DUR (ix), a     ; event duration in 1/32s
    ld      TRK_PTR_L (ix), l
    ld      TRK_PTR_H (ix), h

    ld      a, d
    ld      (fm_cur_note), a
    and     #0x0F
    jp      z, _trk_keyoff      ; rest: just silence

    ;; Velocity: carrier TL = track base + 2 * velocity nibble
    ld      a, d
    rrca
    rrca
    rrca                        ; velocity<<1 in bits 1-4
    and     #0x1E
    add     a, TRK_TL (ix)
    ld      c, a
    ld      a, #0x48            ; carrier OP2 TL
    add     a, TRK_CHOFF (ix)
    ld      b, a
    ld      a, c
    call    fm_track_write
    ld      a, #0x4C            ; carrier OP4 TL
    add     a, TRK_CHOFF (ix)
    ld      b, a
    ld      a, c
    call    fm_track_write

    ;; Auto-pan: cycle C/R/C/L every 4 key-ons (run-line channels)
    ld      a, TRK_FLAGS (ix)
    or      a
    jr      z, _trk_no_pan
    ld      a, TRK_CNT (ix)
    inc     TRK_CNT (ix)
    rrca
    rrca
    and     #0x03
    ld      e, a
    ld      d, #0
    ld      hl, #fm_pan_table
    add     hl, de
    ld      a, #REG_FM_LR_AMS_PMS
    add     a, TRK_CHOFF (ix)
    ld      b, a
    ld      a, (hl)
    call    fm_track_write
_trk_no_pan:
    ;; Look up F-number (2 bytes: block|hi, lo), ids start at 1
    ld      a, (fm_cur_note)
    and     #0x0F
    dec     a
    add     a, a
    ld      e, a
    ld      d, #0
    ld      hl, #fm_note_table
    add     hl, de
    ld      a, (hl)
    ld      d, a                ; D = block | fnum high
    inc     hl
    ld      a, (hl)
    sub     TRK_FNUM_SUB (ix)   ; echo detune, borrows into the high byte
    ld      e, a                ; E = fnum low
    jr      nc, _trk_no_borrow
    dec     d
_trk_no_borrow:

    ;; Retrigger: key off before the new note
    ld      b, #REG_FM_KEYON
    ld      a, TRK_KEYON (ix)
    call    ym_write_a

    ;; Block/F-num high must be written before F-num low
    ld      a, #REG_FM_FNUM_HI
    add     a, TRK_CHOFF (ix)
    ld      b, a
    ld      a, d
    call    fm_track_write
    ld      a, #REG_FM_FNUM_LO
    add     a, TRK_CHOFF (ix)
    ld      b, a
    ld      a, e
    call    fm_track_write

    ;; Key on, all 4 operators
    ld      b, #REG_FM_KEYON
    ld      a, TRK_KEYON (ix)
    or      #0xF0
    jp      ym_write_a

_trk_end:
    xor     a
    ld      TRK_ACTIVE (ix), a
_trk_keyoff:
    ld      b, #REG_FM_KEYON
    ld      a, TRK_KEYON (ix)
    jp      ym_write_a

;;; Write B=register, A=value to this track's port (IX = track state)
fm_track_write:
    push    af
    ld      a, TRK_PORT (ix)
    or      a
    jr      nz, _ftw_port_b
    pop     af
    jp      ym_write_a
_ftw_port_b:
    pop     af
    jp      ym_write_b


;;; === YM2610 Low-Level Access ===

;;; Reset YM2610
ym2610_reset:
    push    bc

    ;; Reset timers
    ld      b, #REG_TIMER_FLAGS
    ld      a, #0x30
    call    ym_write_a

    ;; Key off all FM channels
    call    fm_keyoff_all

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
;;; progear_assets.py will generate these at the known offset
;;; The Makefile patches these after building the driver

    .org    0x0800

;;; SFX table: 32 entries, 4 bytes each (start_l, start_h, stop_l, stop_h)
;;; Addresses are in 256-byte units
sfx_table::
    .ds     128                 ; 32 * 4 bytes

;;; Music table: 32 entries, 6 bytes each (start_l, start_h, stop_l, stop_h, delta_l, delta_h)
music_table::
    .ds     192                 ; 32 * 6 bytes


;;; === FM Boot Music Data (0x0940, after the patched sample tables) ===
;;; The Makefile overwrites 0x0800-0x093F with audio-tables.bin (320 bytes),
;;; so FM data must stay at or after 0x0940.

    .org    0x0940

;;; Register bases written by fm_load_patch (channel offset added at runtime)
;;; Operator register order is OP1 (+0), OP3 (+4), OP2 (+8), OP4 (+0xC)
fm_patch_regs:
    .byte   0x30, 0x34, 0x38, 0x3C      ; DT/MUL
    .byte   0x40, 0x44, 0x48, 0x4C      ; TL
    .byte   0x50, 0x54, 0x58, 0x5C      ; KS/AR
    .byte   0x60, 0x64, 0x68, 0x6C      ; AM/DR
    .byte   0x70, 0x74, 0x78, 0x7C      ; SR
    .byte   0x80, 0x84, 0x88, 0x8C      ; SL/RR
    .byte   0x90, 0x94, 0x98, 0x9C      ; SSG-EG (cleared)
    .byte   0xB0                        ; Feedback/Algorithm
    .byte   0xB4                        ; L/R output

;;; Instrument patches, register-for-register from the pbobblen trace.
;;; Both are algorithm 4 (two 2-op stacks, carriers OP2/OP4).
;;; Value order matches fm_patch_regs: OP1, OP3, OP2, OP4

;;; Melody voice: carriers at MUL 8 and MUL 0.5 - a body one octave below
;;; the written note plus a chime three octaves above it - with heavy
;;; opposed detune (DT 3/7) for chorus. Soft-ish attack (AR 15).
fm_patch_melody:
    .byte   0x33, 0x74, 0x78, 0x30      ; DT/MUL
    .byte   0x15, 0x40, 0x7F, 0x7F      ; TL (carriers written per note)
    .byte   0x0F, 0x0F, 0x0F, 0x0F      ; KS/AR
    .byte   0x0C, 0x0C, 0x0C, 0x05      ; AM/DR
    .byte   0x06, 0x06, 0x05, 0x00      ; SR
    .byte   0x07, 0x97, 0x17, 0x27      ; SL/RR
    .byte   0x00, 0x00, 0x00, 0x00      ; SSG-EG off
    .byte   0x04                        ; FB=0, ALG=4
    .byte   0xC0                        ; both speakers

;;; Run voice: plucky (fast sustain decay on OP4), carriers at MUL 0.5/1,
;;; 6:1 modulator on the second stack for edge, key scaling on carriers.
fm_patch_run:
    .byte   0x00, 0x06, 0x00, 0x01      ; DT/MUL
    .byte   0x14, 0x1A, 0x7F, 0x7F      ; TL (carriers written per note)
    .byte   0x19, 0xD9, 0x59, 0x59      ; KS/AR
    .byte   0x02, 0x02, 0x06, 0x0C      ; AM/DR
    .byte   0x0A, 0x09, 0x0A, 0x0F      ; SR
    .byte   0x14, 0x02, 0x15, 0xA5      ; SL/RR
    .byte   0x00, 0x00, 0x00, 0x00      ; SSG-EG off
    .byte   0x0C                        ; FB=1, ALG=4
    .byte   0xC0                        ; both speakers

;;; Auto-pan cycle for the run channels: C -> R -> C -> L
fm_pan_table:
    .byte   0xC0, 0x40, 0xC0, 0x80

;;; Note table: block|fnum_hi, fnum_lo (F-numbers as traced)
fm_note_table:
    .byte   0x23, 0x9E          ;  1: G4  (fnum 926, block 4)
    .byte   0x24, 0x0E          ;  2: A4  (fnum 1038, block 4)
    .byte   0x2A, 0x6A          ;  3: C5  (fnum 618, block 5)
    .byte   0x2A, 0xB6          ;  4: D5  (fnum 694, block 5)
    .byte   0x2B, 0x0A          ;  5: E5  (fnum 778, block 5)
    .byte   0x2B, 0x39          ;  6: F5  (fnum 825, block 5)
    .byte   0x2B, 0x9E          ;  7: G5  (fnum 926, block 5)
    .byte   0x2C, 0x0E          ;  8: A5  (fnum 1038, block 5)
    .byte   0x2C, 0x4C          ;  9: Bb5 (fnum 1100, block 5)
    .byte   0x2C, 0x8D          ; 10: B5  (fnum 1165, block 5)
    .byte   0x32, 0x6A          ; 11: C6  (fnum 618, block 6)

;;; Note streams. Events: velocity<<4 | note id (0 = rest), duration in
;;; 1/32 notes; 0xFF ends the track. Velocity and rhythm are lifted from
;;; the pbobblen trace: long notes restrike strong/soft (vel 0/4), the
;;; repeated-A bars fade out one velocity step per note.
;;; Melody: G5.G5. E5.E5. B5 B5 | A5 x8 | C6.C6. B5.B5. G5 G5 | A5 x7
fm_song_melody:
    .byte   0x07,6, 0x47,6, 0x05,6, 0x45,6, 0x0A,4, 0x4A,4
    .byte   0x08,4, 0x18,4, 0x28,4, 0x38,4, 0x48,4, 0x58,4, 0x68,4, 0x78,4
    .byte   0x0B,6, 0x4B,6, 0x0A,6, 0x4A,6, 0x07,4, 0x47,4
    .byte   0x08,4, 0x18,4, 0x28,4, 0x38,4, 0x48,4, 0x58,4, 0x68,4
    .byte   0x00,4
    .byte   0xFF

;;; Run line: every 1/16 is a pitch struck twice (strong then vel-4 soft
;;; 1/32s). Four bars following the harmony (G, Am, C, Am).
fm_song_run:
    ;; bar 1: G4 C5 D5 F5 G5 F5 D5 C5  D5 G4 C5 D5 D5 G4 C5 D5
    .byte   0x01,1, 0x41,1, 0x03,1, 0x43,1, 0x04,1, 0x44,1, 0x06,1, 0x46,1
    .byte   0x07,1, 0x47,1, 0x06,1, 0x46,1, 0x04,1, 0x44,1, 0x03,1, 0x43,1
    .byte   0x04,1, 0x44,1, 0x01,1, 0x41,1, 0x03,1, 0x43,1, 0x04,1, 0x44,1
    .byte   0x04,1, 0x44,1, 0x01,1, 0x41,1, 0x03,1, 0x43,1, 0x04,1, 0x44,1
    ;; bar 2: A4 D5 E5 G5 A5 G5 E5 D5  E5 A4 D5 E5 E5 A4 D5 E5
    .byte   0x02,1, 0x42,1, 0x04,1, 0x44,1, 0x05,1, 0x45,1, 0x07,1, 0x47,1
    .byte   0x08,1, 0x48,1, 0x07,1, 0x47,1, 0x05,1, 0x45,1, 0x04,1, 0x44,1
    .byte   0x05,1, 0x45,1, 0x02,1, 0x42,1, 0x04,1, 0x44,1, 0x05,1, 0x45,1
    .byte   0x05,1, 0x45,1, 0x02,1, 0x42,1, 0x04,1, 0x44,1, 0x05,1, 0x45,1
    ;; bar 3: C5 E5 G5 Bb5 C6 Bb5 G5 E5  Bb5 C5 E5 G5 Bb5 C5 E5 G5
    .byte   0x03,1, 0x43,1, 0x05,1, 0x45,1, 0x07,1, 0x47,1, 0x09,1, 0x49,1
    .byte   0x0B,1, 0x4B,1, 0x09,1, 0x49,1, 0x07,1, 0x47,1, 0x05,1, 0x45,1
    .byte   0x09,1, 0x49,1, 0x03,1, 0x43,1, 0x05,1, 0x45,1, 0x07,1, 0x47,1
    .byte   0x09,1, 0x49,1, 0x03,1, 0x43,1, 0x05,1, 0x45,1, 0x07,1, 0x47,1
    ;; bar 4: (A4 D5 E5 A5) x4
    .byte   0x02,1, 0x42,1, 0x04,1, 0x44,1, 0x05,1, 0x45,1, 0x08,1, 0x48,1
    .byte   0x02,1, 0x42,1, 0x04,1, 0x44,1, 0x05,1, 0x45,1, 0x08,1, 0x48,1
    .byte   0x02,1, 0x42,1, 0x04,1, 0x44,1, 0x05,1, 0x45,1, 0x08,1, 0x48,1
    .byte   0x02,1, 0x42,1, 0x04,1, 0x44,1, 0x05,1, 0x45,1, 0x08,1, 0x48,1
    .byte   0xFF

;;; Initial track states, copied to fm_tracks at song start.
;;; The run line leads by 5 ticks (as traced); each echo trails its lead
;;; by 1 tick with a constant -6 F-number detune and +2 carrier TL.
;;; Layout: active, ptr, dur, wait, fnum_sub, keyon, choff, port,
;;;         tl, flags, cnt
fm_track_init:
    .byte   1                   ; track 0: melody, ch1 (port A)
    .word   fm_song_melody
    .byte   0, 5, 0, 0x01, 0x01, 0x00, 0x14, 0x00, 0
    .byte   1                   ; track 1: melody echo, ch2 (port A)
    .word   fm_song_melody
    .byte   0, 6, 6, 0x02, 0x02, 0x00, 0x16, 0x00, 0
    .byte   1                   ; track 2: run line, ch3 (port B)
    .word   fm_song_run
    .byte   0, 0, 0, 0x05, 0x01, 0x01, 0x1A, 0x01, 0
    .byte   1                   ; track 3: run echo, ch4 (port B)
    .word   fm_song_run
    .byte   0, 1, 6, 0x06, 0x02, 0x01, 0x1C, 0x01, 0


;;; === Sequenced Song Engine (0x0C00) ===
;;;
;;; Plays multi-track FM + ADPCM-A songs compiled by tools/ngsong.py. Unlike
;;; the ADPCM-B music path, which streams ~9.25 KB of V-ROM per second, a song
;;; here is note data in M1 ROM: a few KB regardless of length.
;;;
;;; Driven by the same Timer B the boot jingle uses, so the two never play
;;; together - seq_start stops the jingle first.
;;;
;;; Data blob layout (patched in at SONG_BASE by the Makefile):
;;;   +0x0000  song table, 16 x 2-byte absolute pointers (0 = unused)
;;;   +0x0020  instrument records, 32 bytes each (30 patch bytes + carrier mask)
;;;   ...      song headers and event streams
;;;
;;; Song header: timer_b, n_tracks, then 7 bytes per track:
;;;   type (0=FM, 1=drum), channel, instrument, stream word, loop word

    .org    0x0C00

    .equ    SONG_BASE,      0x2000
    .equ    SONG_INST_BASE, SONG_BASE + 0x20
    .equ    SEQ_MAX_TRACKS, 8

;;; Per-track state (16 bytes so the index is a shift, not a multiply)
    .equ    SEQ_ACTIVE,     0
    .equ    SEQ_PTR_L,      1
    .equ    SEQ_PTR_H,      2
    .equ    SEQ_DUR,        3   ; ticks left on the current event
    .equ    SEQ_TYPE,       4   ; 0 = FM, 1 = ADPCM-A drum
    .equ    SEQ_CH,         5   ; FM 0-3, or ADPCM-A channel 0-5
    .equ    SEQ_VOL,        6   ; carrier attenuation (0 loudest)
    .equ    SEQ_LOOP_L,     7
    .equ    SEQ_LOOP_H,     8
    .equ    SEQ_KEYON,      9   ; FM key-on code (1/2/5/6)
    .equ    SEQ_CHOFF,      10  ; FM register offset (1/2)
    .equ    SEQ_PORT,       11  ; 0 = port A, 1 = port B
    .equ    SEQ_CARMASK,    12  ; which TL slots are carriers
    .equ    SEQ_SIZE,       16

;;; Stream opcodes
    .equ    SEQ_OP_REST,    0x60
    .equ    SEQ_OP_INST,    0x61
    .equ    SEQ_OP_VOL,     0x62
    .equ    SEQ_OP_PAN,     0x63
    .equ    SEQ_OP_TIE,     0x64
    .equ    SEQ_OP_END,     0xFF

;;; Write B=register, A=value to this track's port (IX = track state).
;;; The boot jingle's fm_track_write does the same job but reads TRK_PORT at
;;; offset 8, which is SEQ_LOOP_H here - it must not be used on seq tracks.
seq_write:
    push    af
    ld      a, SEQ_PORT (ix)
    or      a
    jr      nz, _seq_write_b
    pop     af
    jp      ym_write_a
_seq_write_b:
    pop     af
    jp      ym_write_b

;;; FM channel index -> key-on code, register offset, port
seq_fm_map:
    .byte   0x01, 1, 0
    .byte   0x02, 2, 0
    .byte   0x05, 1, 1
    .byte   0x06, 2, 1

;;; Start song A (0-15). Main loop context.
seq_start:
    push    bc
    push    de
    push    hl

    ld      (seq_song), a

    ;; The jingle owns Timer B; make sure it is not also running
    call    seq_stop_quiet
    call    fm_stop

    out     (PORT_DISABLE_NMI), a

    ;; HL = song_table[A]
    ld      l, a
    ld      h, #0
    add     hl, hl
    ld      de, #SONG_BASE
    add     hl, de
    ld      e, (hl)
    inc     hl
    ld      d, (hl)
    ld      a, d
    or      e
    jp      z, _seq_start_done      ; empty slot

    ex      de, hl                  ; HL = song header

    ld      a, (hl)                 ; tempo
    ld      (seq_timer_b), a
    inc     hl
    ld      a, (hl)                 ; track count
    inc     hl
    cp      #SEQ_MAX_TRACKS + 1
    jr      c, _seq_count_ok
    ld      a, #SEQ_MAX_TRACKS
_seq_count_ok:
    ld      (seq_ntracks), a
    ld      b, a

    ld      ix, #seq_tracks
_seq_init_track:
    push    bc

    ld      a, (hl)                 ; type
    inc     hl
    ld      SEQ_TYPE (ix), a
    ld      a, (hl)                 ; channel
    inc     hl
    ld      SEQ_CH (ix), a
    ld      c, (hl)                 ; instrument
    inc     hl

    ld      a, (hl)                 ; stream pointer
    inc     hl
    ld      SEQ_PTR_L (ix), a
    ld      a, (hl)
    inc     hl
    ld      SEQ_PTR_H (ix), a
    ld      a, (hl)                 ; loop pointer
    inc     hl
    ld      SEQ_LOOP_L (ix), a
    ld      a, (hl)
    inc     hl
    ld      SEQ_LOOP_H (ix), a

    ld      a, #1
    ld      SEQ_ACTIVE (ix), a
    xor     a
    ld      SEQ_DUR (ix), a
    ld      a, #0x20                ; a sane default until the stream sets one
    ld      SEQ_VOL (ix), a

    ld      a, SEQ_TYPE (ix)
    or      a
    jr      nz, _seq_init_next      ; drum tracks need no FM setup

    ;; Resolve the FM channel map
    push    hl
    ld      a, SEQ_CH (ix)
    ld      l, a
    ld      h, #0
    ld      d, h
    ld      e, l
    add     hl, hl                  ; * 2
    add     hl, de                  ; * 3
    ld      de, #seq_fm_map
    add     hl, de
    ld      a, (hl)
    ld      SEQ_KEYON (ix), a
    inc     hl
    ld      a, (hl)
    ld      SEQ_CHOFF (ix), a
    inc     hl
    ld      a, (hl)
    ld      SEQ_PORT (ix), a
    pop     hl

    ld      a, c
    call    seq_load_instrument

_seq_init_next:
    ld      de, #SEQ_SIZE
    add     ix, de
    pop     bc
    djnz    _seq_init_track

    ;; Start Timer B at the song's tempo
    ld      b, #REG_TIMER_B_LOAD
    ld      a, (seq_timer_b)
    call    ym_write_a
    ld      b, #REG_TIMER_FLAGS
    ld      a, #0x3A                ; reset A+B flags, enable B, load B
    call    ym_write_a

    ld      a, #1
    ld      (seq_active), a

_seq_start_done:
    out     (PORT_ENABLE_NMI), a
    pop     hl
    pop     de
    pop     bc
    ret

;;; Load instrument A into the channel described by IX, caching its carrier mask
seq_load_instrument:
    push    bc
    push    de
    push    hl

    ld      l, a
    ld      h, #0
    add     hl, hl
    add     hl, hl
    add     hl, hl
    add     hl, hl
    add     hl, hl                  ; * 32
    ld      de, #SONG_INST_BASE
    add     hl, de

    push    hl
    ld      de, #30
    add     hl, de
    ld      a, (hl)                 ; carrier mask trails the 30 patch bytes
    ld      SEQ_CARMASK (ix), a
    pop     hl

    ld      c, SEQ_CHOFF (ix)
    ld      a, SEQ_PORT (ix)
    call    fm_load_patch

    pop     hl
    pop     de
    pop     bc
    ret

;;; Write SEQ_VOL to every carrier TL register of the channel (IX = track)
seq_apply_volume:
    push    bc
    push    de
    ld      c, SEQ_CARMASK (ix)
    ld      d, #0x40                ; TL slots: 0x40, 0x44, 0x48, 0x4C
    ld      e, #4
_seq_vol_slot:
    rr      c
    jr      nc, _seq_vol_skip
    ld      a, d
    add     a, SEQ_CHOFF (ix)
    ld      b, a
    ld      a, SEQ_VOL (ix)
    call    seq_write
_seq_vol_skip:
    ld      a, d
    add     a, #4
    ld      d, a
    dec     e
    jr      nz, _seq_vol_slot
    pop     de
    pop     bc
    ret

;;; Poll Timer B and advance every track (main loop context)
seq_poll:
    in      a, (PORT_YM2610_A_ADDR)
    bit     1, a                    ; Timer B expired?
    ret     z

    out     (PORT_DISABLE_NMI), a

    ld      b, #REG_TIMER_FLAGS
    ld      a, #0x2A                ; ack B, keep it enabled and loaded
    call    ym_write_a

    ld      a, (seq_ntracks)
    or      a
    jr      z, _seq_poll_done
    ld      b, a
    ld      ix, #seq_tracks
_seq_poll_track:
    push    bc
    call    seq_track_tick
    ld      de, #SEQ_SIZE
    add     ix, de
    pop     bc
    djnz    _seq_poll_track

_seq_poll_done:
    out     (PORT_ENABLE_NMI), a
    ret

;;; Advance one track by one tick (IX = track state)
seq_track_tick:
    ld      a, SEQ_ACTIVE (ix)
    or      a
    ret     z

    ld      a, SEQ_DUR (ix)
    or      a
    jr      z, _seq_fetch
    dec     SEQ_DUR (ix)
    ret     nz                      ; still sounding
    ;; fall through: duration hit zero, fetch the next event

_seq_fetch:
    ld      l, SEQ_PTR_L (ix)
    ld      h, SEQ_PTR_H (ix)

_seq_fetch_loop:
    ld      a, (hl)
    inc     hl

    cp      #SEQ_OP_END
    jr      nz, _seq_not_end
    ld      l, SEQ_LOOP_L (ix)      ; loop back and keep decoding
    ld      h, SEQ_LOOP_H (ix)
    jr      _seq_fetch_loop

_seq_not_end:
    cp      #SEQ_OP_REST
    jr      nz, _seq_not_rest
    ld      a, (hl)
    inc     hl
    ld      SEQ_DUR (ix), a
    call    seq_note_off
    jr      _seq_store_ptr

_seq_not_rest:
    cp      #SEQ_OP_INST
    jr      nz, _seq_not_inst
    ld      a, (hl)
    inc     hl
    push    hl
    call    seq_load_instrument
    call    seq_apply_volume
    pop     hl
    jr      _seq_fetch_loop

_seq_not_inst:
    cp      #SEQ_OP_VOL
    jr      nz, _seq_not_vol
    ld      a, (hl)
    inc     hl
    ld      SEQ_VOL (ix), a
    ld      a, SEQ_TYPE (ix)
    or      a
    jr      nz, _seq_fetch_loop     ; drums carry volume in the trigger
    push    hl
    call    seq_apply_volume
    pop     hl
    jr      _seq_fetch_loop

_seq_not_vol:
    cp      #SEQ_OP_PAN
    jr      nz, _seq_not_pan
    ld      a, (hl)
    inc     hl
    push    hl
    ld      c, a
    ld      a, SEQ_TYPE (ix)
    or      a
    jr      nz, _seq_pan_done
    ld      a, #REG_FM_LR_AMS_PMS
    add     a, SEQ_CHOFF (ix)
    ld      b, a
    ld      a, c
    call    seq_write
_seq_pan_done:
    pop     hl
    jr      _seq_fetch_loop

_seq_not_pan:
    cp      #SEQ_OP_TIE
    jr      nz, _seq_note
    ld      a, (hl)                 ; hold the sounding note, no retrigger
    inc     hl
    ld      SEQ_DUR (ix), a
    jr      _seq_store_ptr

_seq_note:
    ld      c, a                    ; C = note index
    ld      a, (hl)
    inc     hl
    ld      SEQ_DUR (ix), a
    push    hl
    ld      a, SEQ_TYPE (ix)
    or      a
    jr      nz, _seq_note_drum
    ld      a, c
    call    seq_note_on
    jr      _seq_note_done
_seq_note_drum:
    ld      a, c
    call    seq_drum_trigger
_seq_note_done:
    pop     hl

_seq_store_ptr:
    ld      SEQ_PTR_L (ix), l
    ld      SEQ_PTR_H (ix), h
    ret

;;; Key on note A on this track's FM channel (IX = track state)
seq_note_on:
    push    bc
    push    de
    push    hl

    ;; Stash the note index first: seq_apply_volume clobbers A (it preserves
    ;; BC, so C is a safe place to keep it).
    ld      c, a

    ;; Refresh carrier levels so a volume change lands on this note
    call    seq_apply_volume

    ;; Look up block|fnum_hi, fnum_lo
    ld      l, c
    ld      h, #0
    add     hl, hl
    ld      de, #seq_note_table
    add     hl, de
    ld      d, (hl)
    inc     hl
    ld      e, (hl)

    ;; Retrigger: key off, then set the frequency, then key on
    ld      b, #REG_FM_KEYON
    ld      a, SEQ_KEYON (ix)
    call    ym_write_a

    ld      a, #REG_FM_FNUM_HI      ; must precede the low byte
    add     a, SEQ_CHOFF (ix)
    ld      b, a
    ld      a, d
    call    seq_write
    ld      a, #REG_FM_FNUM_LO
    add     a, SEQ_CHOFF (ix)
    ld      b, a
    ld      a, e
    call    seq_write

    ld      b, #REG_FM_KEYON
    ld      a, SEQ_KEYON (ix)
    or      #0xF0                   ; all four operators
    call    ym_write_a

    pop     hl
    pop     de
    pop     bc
    ret

;;; Key off this track's FM channel (IX = track state); drums ring out
seq_note_off:
    ld      a, SEQ_TYPE (ix)
    or      a
    ret     nz
    push    bc
    ld      b, #REG_FM_KEYON
    ld      a, SEQ_KEYON (ix)
    call    ym_write_a
    pop     bc
    ret

;;; Trigger ADPCM-A sample A on this track's fixed channel (IX = track state)
seq_drum_trigger:
    push    bc
    push    de
    push    hl

    ld      c, SEQ_CH (ix)

    ;; HL = &sfx_table[sample]
    ld      l, a
    ld      h, #0
    add     hl, hl
    add     hl, hl
    ld      de, #sfx_table
    add     hl, de

    ;; Stop the channel before repointing it
    ld      a, c
    call    seq_channel_bit         ; A = 1 << channel
    or      #0x80
    ld      b, #REG_ADPCM_A_CTRL
    call    ym_write_b

    ld      a, #REG_ADPCM_A1_START_L
    add     a, c
    ld      b, a
    ld      a, (hl)
    inc     hl
    call    ym_write_b
    ld      a, #REG_ADPCM_A1_START_H
    add     a, c
    ld      b, a
    ld      a, (hl)
    inc     hl
    call    ym_write_b
    ld      a, #REG_ADPCM_A1_STOP_L
    add     a, c
    ld      b, a
    ld      a, (hl)
    inc     hl
    call    ym_write_b
    ld      a, #REG_ADPCM_A1_STOP_H
    add     a, c
    ld      b, a
    ld      a, (hl)
    call    ym_write_b

    ;; Pan centre, level from the track volume (ADPCM-A level is 0-31, and
    ;; louder is a larger value - the inverse of FM attenuation)
    ld      a, #REG_ADPCM_A1_PAN_VOL
    add     a, c
    ld      b, a
    ld      a, SEQ_VOL (ix)
    cp      #32
    jr      c, _seq_drum_level
    ld      a, #31
_seq_drum_level:
    or      #0xC0                   ; both speakers
    call    ym_write_b

    ld      a, c
    call    seq_channel_bit
    ld      b, #REG_ADPCM_A_CTRL
    call    ym_write_b

    pop     hl
    pop     de
    pop     bc
    ret

;;; A = channel 0-5 -> A = 1 << channel
seq_channel_bit:
    push    bc
    ld      b, a
    ld      a, #1
_seq_bit_shift:
    dec     b                       ; b = -1 (sign set) once the shift is done
    jp      m, _seq_bit_done
    add     a, a
    jr      _seq_bit_shift
_seq_bit_done:
    pop     bc
    ret

;;; Stop the song and silence its channels
seq_stop:
    call    seq_stop_quiet
    push    bc
    ld      b, #REG_TIMER_FLAGS
    ld      a, #0x30                ; reset flags, timers off
    call    ym_write_a
    pop     bc
    ret

;;; Silence tracks without touching the timer (used when restarting)
seq_stop_quiet:
    push    bc
    push    de
    ld      a, (seq_active)
    or      a
    jr      z, _seq_stopq_done

    ld      a, (seq_ntracks)
    or      a
    jr      z, _seq_stopq_clear
    ld      b, a
    ld      ix, #seq_tracks
_seq_stopq_track:
    push    bc
    call    seq_note_off
    xor     a
    ld      SEQ_ACTIVE (ix), a
    ld      de, #SEQ_SIZE
    add     ix, de
    pop     bc
    djnz    _seq_stopq_track

_seq_stopq_clear:
    xor     a
    ld      (seq_active), a
    ld      (seq_ntracks), a
_seq_stopq_done:
    pop     de
    pop     bc
    ret

;;; Block|F-number-high, F-number-low for note indices 0..84 (C1..C8).
;;; Generated by tools/ngsong.py: fnum = freq * 2^20 / (8MHz/144) / 2^(block-1)
seq_note_table:
    .byte   0x04, 0xD3          ;   0: C1
    .byte   0x0A, 0x8E          ;   1: C+1
    .byte   0x0A, 0xB5          ;   2: D1
    .byte   0x0A, 0xDE          ;   3: D+1
    .byte   0x0B, 0x0A          ;   4: E1
    .byte   0x0B, 0x38          ;   5: F1
    .byte   0x0B, 0x69          ;   6: F+1
    .byte   0x0B, 0x9D          ;   7: G1
    .byte   0x0B, 0xD4          ;   8: G+1
    .byte   0x0C, 0x0E          ;   9: A1
    .byte   0x0C, 0x4C          ;  10: A+1
    .byte   0x0C, 0x8D          ;  11: B1
    .byte   0x0C, 0xD3          ;  12: C2
    .byte   0x12, 0x8E          ;  13: C+2
    .byte   0x12, 0xB5          ;  14: D2
    .byte   0x12, 0xDE          ;  15: D+2
    .byte   0x13, 0x0A          ;  16: E2
    .byte   0x13, 0x38          ;  17: F2
    .byte   0x13, 0x69          ;  18: F+2
    .byte   0x13, 0x9D          ;  19: G2
    .byte   0x13, 0xD4          ;  20: G+2
    .byte   0x14, 0x0E          ;  21: A2
    .byte   0x14, 0x4C          ;  22: A+2
    .byte   0x14, 0x8D          ;  23: B2
    .byte   0x14, 0xD3          ;  24: C3
    .byte   0x1A, 0x8E          ;  25: C+3
    .byte   0x1A, 0xB5          ;  26: D3
    .byte   0x1A, 0xDE          ;  27: D+3
    .byte   0x1B, 0x0A          ;  28: E3
    .byte   0x1B, 0x38          ;  29: F3
    .byte   0x1B, 0x69          ;  30: F+3
    .byte   0x1B, 0x9D          ;  31: G3
    .byte   0x1B, 0xD4          ;  32: G+3
    .byte   0x1C, 0x0E          ;  33: A3
    .byte   0x1C, 0x4C          ;  34: A+3
    .byte   0x1C, 0x8D          ;  35: B3
    .byte   0x1C, 0xD3          ;  36: C4
    .byte   0x22, 0x8E          ;  37: C+4
    .byte   0x22, 0xB5          ;  38: D4
    .byte   0x22, 0xDE          ;  39: D+4
    .byte   0x23, 0x0A          ;  40: E4
    .byte   0x23, 0x38          ;  41: F4
    .byte   0x23, 0x69          ;  42: F+4
    .byte   0x23, 0x9D          ;  43: G4
    .byte   0x23, 0xD4          ;  44: G+4
    .byte   0x24, 0x0E          ;  45: A4
    .byte   0x24, 0x4C          ;  46: A+4
    .byte   0x24, 0x8D          ;  47: B4
    .byte   0x24, 0xD3          ;  48: C5
    .byte   0x2A, 0x8E          ;  49: C+5
    .byte   0x2A, 0xB5          ;  50: D5
    .byte   0x2A, 0xDE          ;  51: D+5
    .byte   0x2B, 0x0A          ;  52: E5
    .byte   0x2B, 0x38          ;  53: F5
    .byte   0x2B, 0x69          ;  54: F+5
    .byte   0x2B, 0x9D          ;  55: G5
    .byte   0x2B, 0xD4          ;  56: G+5
    .byte   0x2C, 0x0E          ;  57: A5
    .byte   0x2C, 0x4C          ;  58: A+5
    .byte   0x2C, 0x8D          ;  59: B5
    .byte   0x2C, 0xD3          ;  60: C6
    .byte   0x32, 0x8E          ;  61: C+6
    .byte   0x32, 0xB5          ;  62: D6
    .byte   0x32, 0xDE          ;  63: D+6
    .byte   0x33, 0x0A          ;  64: E6
    .byte   0x33, 0x38          ;  65: F6
    .byte   0x33, 0x69          ;  66: F+6
    .byte   0x33, 0x9D          ;  67: G6
    .byte   0x33, 0xD4          ;  68: G+6
    .byte   0x34, 0x0E          ;  69: A6
    .byte   0x34, 0x4C          ;  70: A+6
    .byte   0x34, 0x8D          ;  71: B6
    .byte   0x34, 0xD3          ;  72: C7
    .byte   0x3A, 0x8E          ;  73: C+7
    .byte   0x3A, 0xB5          ;  74: D7
    .byte   0x3A, 0xDE          ;  75: D+7
    .byte   0x3B, 0x0A          ;  76: E7
    .byte   0x3B, 0x38          ;  77: F7
    .byte   0x3B, 0x69          ;  78: F+7
    .byte   0x3B, 0x9D          ;  79: G7
    .byte   0x3B, 0xD4          ;  80: G+7
    .byte   0x3C, 0x0E          ;  81: A7
    .byte   0x3C, 0x4C          ;  82: A+7
    .byte   0x3C, 0x8D          ;  83: B7
    .byte   0x3C, 0xD3          ;  84: C8


;;; === Data Section ===
;;; Located in Z80 RAM (0xF800-0xFFFF)
    .area   _DATA

seq_song:
    .ds     1                   ; index of the song that was started

seq_active:
    .ds     1                   ; 1 while a sequenced song is playing

seq_ntracks:
    .ds     1

seq_timer_b:
    .ds     1

seq_tracks:
    .ds     SEQ_MAX_TRACKS * SEQ_SIZE

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

pending_pan:
    .ds     1                   ; Pan value for next SFX (0x80=L, 0xC0=C, 0x40=R)

fm_request:
    .ds     1                   ; 1 = BIOS requested boot music (cmd 0x02)

fm_active:
    .ds     1                   ; 1 while the boot jingle is playing

fm_cur_port:
    .ds     1                   ; Port for fm_load_patch writes (0=A, 1=B)

fm_cur_note:
    .ds     1                   ; Note byte of the event being processed

fm_tracks:
    .ds     4 * TRK_SIZE        ; Sequencer track states
