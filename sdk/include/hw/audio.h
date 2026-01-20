/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file hw/audio.h
 * @brief Audio service - Z80 sound driver communication.
 * @internal This is an internal header - not for game developers.
 *
 * The NeoGeo uses a Z80 CPU for audio. The 68000 communicates with it
 * by writing command bytes to IO_SOUND. The Z80 acknowledges by echoing
 * the command with bit 7 set.
 */

#ifndef HW_AUDIO_H
#define HW_AUDIO_H

#include <types.h>

/*
 * ============================================================================
 *  AUDIO COMMANDS
 * ============================================================================
 *
 * Command codes sent to the Z80 audio driver.
 * These must match the definitions in the Z80 driver code.
 */

#define AUDIO_CMD_NOP         0x00
#define AUDIO_CMD_SLOT_SWITCH 0x01
#define AUDIO_CMD_EYECATCHER  0x02
#define AUDIO_CMD_RESET       0x03

/* SFX: 0x10-0x1F = SFX 0-15, 0x40-0x4F = SFX 16-31 */
#define AUDIO_CMD_SFX_BASE     0x10
#define AUDIO_CMD_SFX_EXT_BASE 0x40

/* Music: 0x20-0x2F = Music 0-15, 0x50-0x5F = Music 16-31 */
#define AUDIO_CMD_MUSIC_BASE     0x20
#define AUDIO_CMD_MUSIC_EXT_BASE 0x50

/* Music control */
#define AUDIO_CMD_MUSIC_STOP   0x30
#define AUDIO_CMD_MUSIC_PAUSE  0x31
#define AUDIO_CMD_MUSIC_RESUME 0x32

/* SFX channel stop: 0x60-0x65 = Stop channel 0-5 */
#define AUDIO_CMD_SFX_STOP_BASE 0x60

/* Global */
#define AUDIO_CMD_STOP_ALL 0x70

/* Volume: 0x80-0x8F = Volume 0-15 */
#define AUDIO_CMD_VOLUME_BASE 0x80

/*
 * ============================================================================
 *  AUDIO LIMITS
 * ============================================================================
 */

#define AUDIO_MAX_SFX      32
#define AUDIO_MAX_MUSIC    32
#define AUDIO_MAX_CHANNELS 6
#define AUDIO_MAX_VOLUME   15

/**
 * Initialize audio system.
 * Resets the Z80 driver and sets default volume.
 */
void hw_audio_init(void);

/**
 * Send command to Z80 and wait for acknowledgment.
 * Blocks until the Z80 echoes the command.
 *
 * @param cmd Command byte
 */
void hw_audio_command(u8 cmd);

/**
 * Send command to Z80 without waiting.
 * Use when you don't need confirmation (e.g., rapid fire SFX).
 *
 * @param cmd Command byte
 */
void hw_audio_command_async(u8 cmd);

/**
 * Play a sound effect.
 * @param index SFX index (0-31)
 */
void hw_audio_play_sfx(u8 index);

/**
 * Stop a specific SFX channel.
 * @param channel Channel (0-5)
 */
void hw_audio_stop_sfx(u8 channel);

/**
 * Play music (loops indefinitely).
 * @param index Music index (0-31)
 */
void hw_audio_play_music(u8 index);

/**
 * Stop music playback.
 */
void hw_audio_stop_music(void);

/**
 * Pause music playback.
 */
void hw_audio_pause_music(void);

/**
 * Resume paused music.
 */
void hw_audio_resume_music(void);

/**
 * Set master volume.
 * @param volume Volume level (0-15)
 */
void hw_audio_set_volume(u8 volume);

/**
 * Stop all audio (SFX and music).
 */
void hw_audio_stop_all(void);

/**
 * Check if music is currently playing.
 */
u8 hw_audio_music_playing(void);

/**
 * Check if music is paused.
 */
u8 hw_audio_music_paused(void);

/**
 * Get current music index.
 * @return Music index or 0xFF if none playing
 */
u8 hw_audio_get_music(void);

/**
 * Get current volume level.
 */
u8 hw_audio_get_volume(void);

#endif /* HW_AUDIO_H */
