#ifndef AUDIO_SYSTEM_H
#define AUDIO_SYSTEM_H

#include <stdint.h>
#include <alsa/asoundlib.h>

struct audio_ctx_t {
    snd_pcm_t *pcm_handle;
    int volume;
    int is_8bit;
    int channels;
    int16_t last_l, last_r;
};

typedef struct audio_ctx_t audio_ctx_t;

audio_ctx_t* audio_init(int sample_rate, int channels, int volume, int is_8bit);
void audio_play_stereo32(audio_ctx_t* ctx, uint32_t* buffer, int samples_count);
void audio_play_mono8(audio_ctx_t* ctx, uint8_t* buffer, int samples_count);
void audio_set_volume(audio_ctx_t* ctx, int volume);
void audio_close(audio_ctx_t* ctx);
#endif