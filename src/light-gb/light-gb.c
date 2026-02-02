#define _GNU_SOURCE
#define ENABLE_SOUND 1
#define ENABLE_LCD 1

/* --- configs --- */
#define INPUT_DEVICE_PATH "/dev/input/event3" 
#define AUDIO_SAMPLE_RATE 44100
#define SAVE_FILE_EXTENSION ".sav"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <alsa/asoundlib.h>

uint8_t audio_read(const uint16_t addr);
void audio_write(const uint16_t addr, const uint8_t val);

#include "peanut_gb.h"
#include "minigb_apu/minigb_apu.h"
#include "MiniFB.h"

#ifndef JOYPAD_START
#define JOYPAD_RIGHT  0x01
#define JOYPAD_LEFT   0x02
#define JOYPAD_UP     0x04
#define JOYPAD_DOWN   0x08
#define JOYPAD_A      0x10
#define JOYPAD_B      0x20
#define JOYPAD_SELECT 0x40
#define JOYPAD_START  0x80
#endif

static struct minigb_apu_ctx apu_ctx;
static snd_pcm_t *pcm_handle = NULL;
static audio_sample_t *audio_buffer = NULL;
static int input_fd = -1;

struct priv_t
{
    uint8_t *rom;
    uint8_t *cart_ram;
    size_t save_size;
    uint32_t fb[LCD_HEIGHT][LCD_WIDTH];
    uint32_t palette[4]; 
};

static struct priv_t priv;

const uint32_t palettes[][4] = {
    { 0xFFFFFF, 0xA5A5A5, 0x525252, 0x000000 },
    { 0xE0F8D0, 0x88C070, 0x346856, 0x081820 },
    { 0xFFFFB5, 0x7BC67B, 0x6B8C42, 0x5A3921 },
    { 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF }
};
int current_palette_idx = 0;

uint8_t audio_read(const uint16_t addr) { return minigb_apu_audio_read(&apu_ctx, addr); }
void audio_write(const uint16_t addr, const uint8_t val) { minigb_apu_audio_write(&apu_ctx, addr, val); }

uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) { return priv.rom[addr]; }
uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) { return priv.cart_ram[addr]; }
void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) { priv.cart_ram[addr] = val; }
void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t val) {
    fprintf(stderr, "GB Error %d at 0x%04X\n", gb_err, val);
    exit(1);
}

uint8_t *read_file(const char *filename, size_t *size_out) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    uint8_t *buf = malloc(sz);
    if (buf) fread(buf, 1, sz, f);
    if (size_out) *size_out = sz;
    fclose(f);
    return buf;
}

void handle_save(const char *rom_path, int save) {
    if (priv.save_size == 0) return;
    char save_path[256];
    strncpy(save_path, rom_path, sizeof(save_path)-5);
    char *dot = strrchr(save_path, '.');
    if (dot) *dot = '\0';
    strcat(save_path, SAVE_FILE_EXTENSION);

    if (save) {
        if (!priv.cart_ram) return;
        FILE *f = fopen(save_path, "wb");
        if (f) {
            fwrite(priv.cart_ram, 1, priv.save_size, f);
            fclose(f);
            printf("[System] Saved game to %s\n", save_path);
        }
    } else {
        priv.cart_ram = malloc(priv.save_size);
        if (!priv.cart_ram) return;
        memset(priv.cart_ram, 0, priv.save_size);
        FILE *f = fopen(save_path, "rb");
        if (f) {
            fread(priv.cart_ram, 1, priv.save_size, f);
            fclose(f);
            printf("[System] Loaded save from %s\n", save_path);
        }
    }
}

void input_init() {
    input_fd = open(INPUT_DEVICE_PATH, O_RDONLY | O_NONBLOCK);
    if (input_fd < 0) {
        fprintf(stderr, "[Input] Failed to open %s: %s\n", INPUT_DEVICE_PATH, strerror(errno));
    } else {
        printf("[Input] Opened %s successfully.\n", INPUT_DEVICE_PATH);
    }
}

int process_input(struct gb_s *gb, int *fast_mode) {
    struct input_event ev;
    if (input_fd < 0) return 0;

    while (read(input_fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY) {
            int pressed = ev.value;
            if (ev.value == 2) continue; 

            #define KEY_ACTION(mask) do { \
                if (pressed) gb->direct.joypad &= ~(mask); \
                else         gb->direct.joypad |= (mask); \
            } while(0)

            switch (ev.code) {
                case KEY_UP:    KEY_ACTION(JOYPAD_UP); break;
                case KEY_DOWN:  KEY_ACTION(JOYPAD_DOWN); break;
                case KEY_LEFT:  KEY_ACTION(JOYPAD_LEFT); break;
                case KEY_RIGHT: KEY_ACTION(JOYPAD_RIGHT); break;
                case KEY_Z:         KEY_ACTION(JOYPAD_A); break;
                case KEY_X:         KEY_ACTION(JOYPAD_B); break;
                case KEY_ENTER:     KEY_ACTION(JOYPAD_START); break;
                case KEY_BACKSPACE: KEY_ACTION(JOYPAD_SELECT); break;

                case KEY_SPACE: 
                    *fast_mode = pressed ? 3 : 1;
                    break;
                
                case KEY_R:
                    if (pressed) {
                        gb_reset(gb);
                        printf("[System] Reset\n");
                    }
                    break;
                
                case KEY_P:
                    if (pressed) {
                        current_palette_idx = (current_palette_idx + 1) % 4;
                        memcpy(priv.palette, palettes[current_palette_idx], sizeof(priv.palette));
                        printf("[System] Palette: %d\n", current_palette_idx);
                    }
                    break;

                case KEY_ESC:
                    if (pressed) return 1;
                    break;
            }
        }
    }
    return 0;
}

#if ENABLE_LCD
void lcd_draw_line(struct gb_s *gb, const uint8_t pixels[160], const uint_fast8_t line)
{
    for(unsigned int x = 0; x < LCD_WIDTH; x++)
        priv.fb[line][x] = priv.palette[pixels[x] & 3];
}
#endif

int audio_init(void) {
    int err;
    const char *devs[] = {"default", "plughw:0,0", "plughw:1,0", NULL};
    for(int i=0; devs[i]; i++) {
        if (snd_pcm_open(&pcm_handle, devs[i], SND_PCM_STREAM_PLAYBACK, 0) >= 0) {
            printf("[Audio] Opened %s\n", devs[i]);
            break;
        }
    }
    if (!pcm_handle) return -1;
    snd_pcm_set_params(pcm_handle, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 2, AUDIO_SAMPLE_RATE, 1, 50000);
    return 0;
}

int main(int argc, char **argv)
{
    struct gb_s gb;
    enum gb_init_error_e ret;
    int fast_mode = 1;

    if(argc != 2) {
        printf("Usage: %s ROM_FILE\n", argv[0]);
        return 1;
    }

    priv.rom = read_file(argv[1], NULL);
    if (!priv.rom) { printf("Failed to read ROM\n"); return 1; }

    ret = gb_init(&gb, &gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write, &gb_error, &priv);
    if(ret != GB_INIT_NO_ERROR) { printf("GB Init Error: %d\n", ret); return 1; }

    gb_get_save_size_s(&gb, &priv.save_size);
    handle_save(argv[1], 0);
    
    time_t rawtime; time(&rawtime);
    gb_set_rtc(&gb, localtime(&rawtime));

    memcpy(priv.palette, palettes[0], sizeof(priv.palette));
#if ENABLE_LCD
    gb_init_lcd(&gb, &lcd_draw_line);
#endif
    if (mfb_open("Peanut-GB", LCD_WIDTH, LCD_HEIGHT) < 0) return 1;
    
    if (audio_init() == 0) {
        minigb_apu_audio_init(&apu_ctx);
        audio_buffer = malloc(AUDIO_SAMPLES_TOTAL * sizeof(audio_sample_t));
    } else {
        printf("[Warning] Audio init failed\n");
    }

    input_init();

    printf("[System] Started. Keys: Z=A, X=B, Enter=Start, Back=Select, Space=Fast, R=Reset, P=Palette, ESC=Quit\n");

    while(1) {
        if (process_input(&gb, &fast_mode)) break;
        gb_run_frame(&gb);

        static int skip_counter = 0;
        int should_render = 1;
        
        if (fast_mode > 1) {
            skip_counter++;
            if (skip_counter < fast_mode) {
                should_render = 0;
            } else {
                skip_counter = 0;
            }
        }

        if (should_render) {
            if (pcm_handle && audio_buffer) {
                minigb_apu_audio_callback(&apu_ctx, audio_buffer);
                snd_pcm_sframes_t f = snd_pcm_writei(pcm_handle, audio_buffer, AUDIO_SAMPLES_TOTAL/2);
                if (f < 0) snd_pcm_recover(pcm_handle, f, 0);
            } else {
                usleep(16000);
            }

            if (mfb_update(priv.fb) < 0) break;
        }
    }

    handle_save(argv[1], 1);
    mfb_close();
    if (pcm_handle) snd_pcm_close(pcm_handle);
    if (audio_buffer) free(audio_buffer);
    free(priv.rom);
    free(priv.cart_ram);
    if (input_fd > 0) close(input_fd);

    return 0;
}