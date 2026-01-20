/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

#include <hw/audio.h>
#include <hw/io.h>

/* Internal state */
static u8 current_music = 0xFF;
static u8 music_paused = 0;
static u8 master_volume = 15;

void hw_audio_command(u8 cmd) {
    u8 reply;
    u16 timeout;

    IO_SOUND = cmd;

    /* Wait for Z80 acknowledgment (echoes command with bit 7 set) */
    timeout = 0xFFFF;
    do {
        reply = IO_SOUND;
        timeout--;
    } while ((reply != (cmd | 0x80)) && (timeout > 0));
}

void hw_audio_command_async(u8 cmd) {
    IO_SOUND = cmd;
}

void hw_audio_init(void) {
    hw_audio_command(AUDIO_CMD_RESET);
    master_volume = 15;
    hw_audio_set_volume(master_volume);
    current_music = 0xFF;
    music_paused = 0;
}

void hw_audio_play_sfx(u8 index) {
    if (index >= AUDIO_MAX_SFX)
        return;

    if (index < 16) {
        hw_audio_command(AUDIO_CMD_SFX_BASE + index);
    } else {
        hw_audio_command(AUDIO_CMD_SFX_EXT_BASE + (index - 16));
    }
}

void hw_audio_stop_sfx(u8 channel) {
    if (channel >= AUDIO_MAX_CHANNELS)
        return;
    hw_audio_command(AUDIO_CMD_SFX_STOP_BASE + channel);
}

void hw_audio_play_music(u8 index) {
    if (index >= AUDIO_MAX_MUSIC)
        return;

    current_music = index;
    music_paused = 0;

    if (index < 16) {
        hw_audio_command(AUDIO_CMD_MUSIC_BASE + index);
    } else {
        hw_audio_command(AUDIO_CMD_MUSIC_EXT_BASE + (index - 16));
    }
}

void hw_audio_stop_music(void) {
    hw_audio_command(AUDIO_CMD_MUSIC_STOP);
    current_music = 0xFF;
    music_paused = 0;
}

void hw_audio_pause_music(void) {
    if (current_music != 0xFF && !music_paused) {
        hw_audio_command(AUDIO_CMD_MUSIC_PAUSE);
        music_paused = 1;
    }
}

void hw_audio_resume_music(void) {
    if (music_paused) {
        hw_audio_command(AUDIO_CMD_MUSIC_RESUME);
        music_paused = 0;
    }
}

void hw_audio_set_volume(u8 volume) {
    if (volume > AUDIO_MAX_VOLUME)
        volume = AUDIO_MAX_VOLUME;
    master_volume = volume;
    hw_audio_command(AUDIO_CMD_VOLUME_BASE + volume);
}

void hw_audio_stop_all(void) {
    hw_audio_command(AUDIO_CMD_STOP_ALL);
    current_music = 0xFF;
    music_paused = 0;
}

u8 hw_audio_music_playing(void) {
    return (current_music != 0xFF && !music_paused) ? 1 : 0;
}

u8 hw_audio_music_paused(void) {
    return music_paused;
}

u8 hw_audio_get_music(void) {
    return current_music;
}

u8 hw_audio_get_volume(void) {
    return master_volume;
}
