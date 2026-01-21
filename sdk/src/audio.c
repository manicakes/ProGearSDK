/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file audio.c
 * @brief Public audio API - thin wrapper over hw/audio.
 */

#include <audio.h>
#include <hw/audio.h>
#include <actor.h>
#include <camera.h>

void AudioPlaySfx(u8 sfx_index) {
    hw_audio_play_sfx(sfx_index);
}

void AudioPlaySfxAt(Actor actor, u8 sfx_index) {
    /* Calculate pan based on actor screen position */
    fixed actor_x = ActorGetX(actor);
    fixed camera_x = CameraGetX();
    s32 screen_x = (actor_x - camera_x) >> 16;

    /* Simple left/center/right panning based on screen thirds */
    (void)screen_x; /* Pan not yet implemented in Z80 driver */
    hw_audio_play_sfx(sfx_index);
}

void AudioStopSfx(u8 channel) {
    hw_audio_stop_sfx(channel);
}

void AudioStopAllSfx(void) {
    for (u8 i = 0; i < AUDIO_CHANNELS; i++) {
        hw_audio_stop_sfx(i);
    }
}

void AudioPlayMusic(u8 music_index) {
    hw_audio_play_music(music_index);
}

void AudioStopMusic(void) {
    hw_audio_stop_music();
}

void AudioPauseMusic(void) {
    hw_audio_pause_music();
}

void AudioResumeMusic(void) {
    hw_audio_resume_music();
}

u8 AudioMusicPlaying(void) {
    return hw_audio_music_playing();
}

u8 AudioMusicPaused(void) {
    return hw_audio_music_paused();
}

u8 AudioGetMusic(void) {
    return hw_audio_get_music();
}

void AudioSetVolume(u8 volume) {
    hw_audio_set_volume(volume);
}

u8 AudioGetVolume(void) {
    return hw_audio_get_volume();
}

void AudioStopAll(void) {
    hw_audio_stop_all();
}

void AudioInit(void) {
    hw_audio_init();
}
