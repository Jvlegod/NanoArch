#include "audio.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdio.h>

audio_ctx_t* audio_init(int sample_rate, int channels, int volume, int is_8bit) {
    audio_ctx_t* ctx = (audio_ctx_t*)calloc(1, sizeof(audio_ctx_t));
    int err;
    const char *devs[] = { "default", "plughw:0,0", "plughw:1,0" };
    
    for (int i = 0; i < 3; i++) {
        err = snd_pcm_open(&ctx->pcm_handle, devs[i], SND_PCM_STREAM_PLAYBACK, 0);
        if (err >= 0) {
            printf("[Audio] Successfully opened %s\n", devs[i]);
            break;
        }
        fprintf(stderr, "[Audio Warning] Failed to open %s: %s\n", devs[i], snd_strerror(err));
    }

    if (err < 0) {
        fprintf(stderr, "[Audio Error] All devices failed. Final error: %s\n", snd_strerror(err));
        free(ctx);
        return NULL;
    }

    snd_pcm_set_params(ctx->pcm_handle, is_8bit ? SND_PCM_FORMAT_U8 : SND_PCM_FORMAT_S16,
                       SND_PCM_ACCESS_RW_INTERLEAVED, channels, sample_rate, 1, 50000);
    
    ctx->volume = volume;
    ctx->is_8bit = is_8bit;
    ctx->channels = channels;
    return ctx;
}

void audio_play_stereo32(audio_ctx_t* ctx, uint32_t* buffer, int samples_count) {
    if (!ctx || ctx->is_8bit) return;

    for (int i = 0; i < samples_count; i++) {
        int16_t cur_l = (int16_t)(buffer[i] & 0xFFFF);
        int16_t cur_r = (int16_t)((buffer[i] >> 16) & 0xFFFF);
        int16_t out_l = (int16_t)(((cur_l * ctx->volume) / 100 + ctx->last_l) >> 1);
        int16_t out_r = (int16_t)(((cur_r * ctx->volume) / 100 + ctx->last_r) >> 1);
        
        ctx->last_l = out_l; ctx->last_r = out_r;
        buffer[i] = (uint32_t)((uint16_t)out_r << 16) | (uint16_t)out_l;
    }
    snd_pcm_writei(ctx->pcm_handle, buffer, samples_count);
}

void audio_play_mono8(audio_ctx_t* ctx, uint8_t* buffer, int samples_count) {
    if (!ctx || !ctx->is_8bit) return;

    for (int i = 0; i < samples_count; i++) {
        int centered = buffer[i] - 128;
        int vol_adj = (centered * ctx->volume) / 100 + 128;
        buffer[i] = (uint8_t)(vol_adj > 255 ? 255 : (vol_adj < 0 ? 0 : vol_adj));
    }
    snd_pcm_writei(ctx->pcm_handle, buffer, samples_count);
}

void audio_set_volume(audio_ctx_t* ctx, int volume) {
    if (!ctx) return;

    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    ctx->volume = volume;
}

void audio_close(audio_ctx_t* ctx) {
    if (ctx) {
        if (ctx->pcm_handle) snd_pcm_close(ctx->pcm_handle);
        free(ctx);
    }
}