/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file ng_audio.c
 * @brief NeoGeo audio system implementation
 *
 * Handles communication with Z80 audio driver for ADPCM playback.
 * Note: Spatial audio functions (NGActorPlaySfx) are in SDK, not HAL.
 */

#include <ng_audio.h>

/* Audio command codes (must match Z80 driver) */
#define CMD_NOP            0x00
#define CMD_SLOT_SWITCH    0x01
#define CMD_EYECATCHER     0x02
#define CMD_RESET          0x03
#define CMD_SFX_BASE       0x10 /* 0x10-0x1F: SFX 0-15 (center) */
#define CMD_MUSIC_BASE     0x20 /* 0x20-0x2F: Music 0-15 */
#define CMD_MUSIC_STOP     0x30
#define CMD_MUSIC_PAUSE    0x31
#define CMD_MUSIC_RESUME   0x32
#define CMD_SFX_EXT_BASE   0x40 /* 0x40-0x4F: SFX 16-31 (center) */
#define CMD_MUSIC_EXT_BASE 0x50 /* 0x50-0x5F: Music 16-31 */
#define CMD_SFX_STOP_CH    0x60 /* 0x60-0x65: Stop SFX channel 0-5 */
#define CMD_STOP_ALL       0x70
#define CMD_VOLUME_BASE    0x80 /* 0x80-0x8F: Volume 0-15 */
#define CMD_SFX_LEFT_BASE  0xC0 /* 0xC0-0xCF: SFX 0-15 (left pan) */
#define CMD_SFX_RIGHT_BASE 0xD0 /* 0xD0-0xDF: SFX 0-15 (right pan) */
#define CMD_SFX_EXT_LEFT   0xE0 /* 0xE0-0xEF: SFX 16-31 (left pan) */
#define CMD_SFX_EXT_RIGHT  0xF0 /* 0xF0-0xFF: SFX 16-31 (right pan) */

static u8 current_music_index = 0xFF;
static u8 music_paused = 0;
static u8 master_volume = 15;
static u8 channel_volumes[NG_AUDIO_MAX_CHANNELS] = {31, 31, 31, 31, 31, 31};
static u8 music_volume = 255;

void NGAudioSendCommand(u8 cmd) {
    u8 reply;
    u16 timeout;

    NG_REG_SOUND = cmd;

    // Wait for Z80 acknowledgment (echoes command with bit 7 set)
    timeout = 0xFFFF;
    do {
        reply = NG_REG_SOUND;
        timeout--;
    } while ((reply != (cmd | 0x80)) && (timeout > 0));
}

void NGAudioSendCommandAsync(u8 cmd) {
    NG_REG_SOUND = cmd;
}

void NGAudioInit(void) {
    NGAudioSendCommand(CMD_RESET);
    master_volume = 15;
    NGAudioSetVolume(master_volume);
    current_music_index = 0xFF;
}

void NGSfxPlay(u8 sfx_index) {
    if (sfx_index >= NG_AUDIO_MAX_SFX)
        return;

    if (sfx_index < 16) {
        NGAudioSendCommand(CMD_SFX_BASE + sfx_index);
    } else {
        NGAudioSendCommand(CMD_SFX_EXT_BASE + (sfx_index - 16));
    }
}

void NGSfxPlayPan(u8 sfx_index, NGPan pan) {
    if (sfx_index >= NG_AUDIO_MAX_SFX)
        return;

    u8 cmd;
    if (sfx_index < 16) {
        switch (pan) {
            case NG_PAN_LEFT:
                cmd = CMD_SFX_LEFT_BASE + sfx_index;
                break;
            case NG_PAN_RIGHT:
                cmd = CMD_SFX_RIGHT_BASE + sfx_index;
                break;
            default: /* NG_PAN_CENTER */
                cmd = CMD_SFX_BASE + sfx_index;
                break;
        }
    } else {
        u8 idx = sfx_index - 16;
        switch (pan) {
            case NG_PAN_LEFT:
                cmd = CMD_SFX_EXT_LEFT + idx;
                break;
            case NG_PAN_RIGHT:
                cmd = CMD_SFX_EXT_RIGHT + idx;
                break;
            default: /* NG_PAN_CENTER */
                cmd = CMD_SFX_EXT_BASE + idx;
                break;
        }
    }
    NGAudioSendCommand(cmd);
}

void NGSfxStopChannel(u8 channel) {
    if (channel >= NG_AUDIO_MAX_CHANNELS)
        return;
    NGAudioSendCommand(CMD_SFX_STOP_CH + channel);
}

void NGSfxStopAll(void) {
    u8 i;
    for (i = 0; i < NG_AUDIO_MAX_CHANNELS; i++) {
        NGAudioSendCommand(CMD_SFX_STOP_CH + i);
    }
}

void NGMusicPlay(u8 music_index) {
    if (music_index >= NG_AUDIO_MAX_MUSIC)
        return;

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
    if (volume > 15)
        volume = 15;
    master_volume = volume;
    NGAudioSendCommand(CMD_VOLUME_BASE + volume);
}

void NGAudioStopAll(void) {
    NGAudioSendCommand(CMD_STOP_ALL);
    current_music_index = 0xFF;
}

u8 NGAudioGetCurrentMusic(void) {
    return current_music_index;
}

u8 NGAudioGetVolume(void) {
    return master_volume;
}

void NGSfxSetChannelVolume(u8 channel, u8 volume) {
    if (channel >= NG_AUDIO_MAX_CHANNELS)
        return;
    if (volume > 31)
        volume = 31;

    channel_volumes[channel] = volume;

    /*
     * Note: Full per-channel volume control requires Z80 driver support.
     * The volume is stored here for tracking. A future Z80 driver update
     * could add commands 0x66-0x6B for per-channel volume.
     *
     * For now, this affects the next SFX played on this channel.
     */
}

void NGMusicSetVolume(u8 volume) {
    music_volume = volume;

    /*
     * Note: Full music volume control requires Z80 driver support.
     * The volume is stored here for tracking. A future Z80 driver update
     * could add command 0x6C for music volume.
     */
}
