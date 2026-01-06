/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file audio.c
 * @brief NeoGeo audio system implementation
 *
 * Handles communication with Z80 audio driver for ADPCM playback.
 */

#include "audio.h"
#include "camera.h"
#include "ngmath.h"

/* Hardware registers for 68k/Z80 communication */
#define REG_SOUND       (*(vu8*)0x320000)   /* Write: send command, Read: get reply */
#define REG_SOUND_REPLY (*(vu8*)0x320001)   /* Read only: Z80 reply */

/* Audio command codes (must match Z80 driver) */
#define CMD_NOP             0x00
#define CMD_SLOT_SWITCH     0x01
#define CMD_EYECATCHER      0x02
#define CMD_RESET           0x03
#define CMD_SFX_BASE        0x10    /* 0x10-0x1F: SFX 0-15 */
#define CMD_MUSIC_BASE      0x20    /* 0x20-0x2F: Music 0-15 */
#define CMD_MUSIC_STOP      0x30
#define CMD_MUSIC_PAUSE     0x31
#define CMD_MUSIC_RESUME    0x32
#define CMD_SFX_EXT_BASE    0x40    /* 0x40-0x4F: SFX 16-31 */
#define CMD_MUSIC_EXT_BASE  0x50    /* 0x50-0x5F: Music 16-31 */
#define CMD_SFX_STOP_CH     0x60    /* 0x60-0x65: Stop SFX channel 0-5 */
#define CMD_STOP_ALL        0x70
#define CMD_VOLUME_BASE     0x80    /* 0x80-0x8F: Volume 0-15 */

/* Screen dimensions for pan calculation */
#define SCREEN_WIDTH        320
#define SCREEN_HALF_WIDTH   160

static u8 current_music_index = 0xFF;
static u8 music_paused = 0;
static u8 master_volume = 15;

void NGAudioSendCommand(u8 cmd) {
    u8 reply;
    u16 timeout;

    REG_SOUND = cmd;

    // Wait for Z80 acknowledgment (echoes command with bit 7 set)
    timeout = 0xFFFF;
    do {
        reply = REG_SOUND;
        timeout--;
    } while ((reply != (cmd | 0x80)) && (timeout > 0));
}

void NGAudioSendCommandAsync(u8 cmd) {
    REG_SOUND = cmd;
}

void NGAudioInit(void) {
    NGAudioSendCommand(CMD_RESET);
    master_volume = 15;
    NGAudioSetVolume(master_volume);
    current_music_index = 0xFF;
}

void NGSfxPlay(u8 sfx_index) {
    if (sfx_index >= NG_AUDIO_MAX_SFX) return;

    if (sfx_index < 16) {
        NGAudioSendCommand(CMD_SFX_BASE + sfx_index);
    } else {
        NGAudioSendCommand(CMD_SFX_EXT_BASE + (sfx_index - 16));
    }
}

void NGSfxPlayPan(u8 sfx_index, NGPan pan) {
    (void)pan;  // TODO: Implement pan in Z80 driver
    NGSfxPlay(sfx_index);
}

void NGSfxStopChannel(u8 channel) {
    if (channel >= NG_AUDIO_MAX_CHANNELS) return;
    NGAudioSendCommand(CMD_SFX_STOP_CH + channel);
}

void NGSfxStopAll(void) {
    u8 i;
    for (i = 0; i < NG_AUDIO_MAX_CHANNELS; i++) {
        NGAudioSendCommand(CMD_SFX_STOP_CH + i);
    }
}

void NGMusicPlay(u8 music_index) {
    if (music_index >= NG_AUDIO_MAX_MUSIC) return;

    current_music_index = music_index;
    music_paused = 0;

    if (music_index < 16) {
        NGAudioSendCommand(CMD_MUSIC_BASE + music_index);
    } else {
        NGAudioSendCommand(CMD_MUSIC_EXT_BASE + (music_index - 16));
    }
}

void NGMusicStop(void) {
    NGAudioSendCommand(CMD_MUSIC_STOP);
    current_music_index = 0xFF;
    music_paused = 0;
}

void NGMusicPause(void) {
    if (current_music_index != 0xFF && !music_paused) {
        NGAudioSendCommand(CMD_MUSIC_PAUSE);
        music_paused = 1;
    }
}

void NGMusicResume(void) {
    if (music_paused) {
        NGAudioSendCommand(CMD_MUSIC_RESUME);
        music_paused = 0;
    }
}

u8 NGMusicIsPlaying(void) {
    return (current_music_index != 0xFF && !music_paused) ? 1 : 0;
}

u8 NGMusicIsPaused(void) {
    return music_paused;
}

void NGAudioSetVolume(u8 volume) {
    if (volume > 15) volume = 15;
    master_volume = volume;
    NGAudioSendCommand(CMD_VOLUME_BASE + volume);
}

void NGAudioStopAll(void) {
    NGAudioSendCommand(CMD_STOP_ALL);
    current_music_index = 0xFF;
}

static NGPan calculate_pan(fixed actor_x, fixed camera_x) {
    s32 screen_x = (actor_x - camera_x) >> 16;
    s32 center_offset = screen_x - SCREEN_HALF_WIDTH;

    if (center_offset < -80) {
        return NG_PAN_LEFT;
    } else if (center_offset > 80) {
        return NG_PAN_RIGHT;
    } else {
        return NG_PAN_CENTER;
    }
}

void NGActorPlaySfx(NGActorHandle actor, u8 sfx_index) {
    fixed actor_x = NGActorGetX(actor);
    fixed camera_x = NGCameraGetX();
    NGPan pan = calculate_pan(actor_x, camera_x);
    NGSfxPlayPan(sfx_index, pan);
}

u8 NGAudioGetCurrentMusic(void) {
    return current_music_index;
}

u8 NGAudioGetVolume(void) {
    return master_volume;
}
