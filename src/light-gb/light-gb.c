#define _GNU_SOURCE
#define ENABLE_SOUND 1
#define ENABLE_LCD 1

/* --- configs --- */
// keyboard
#define INPUT_DEVICE_PATH "/dev/input/event0" 
// joystick
// #define JOYSTICK_DEVICE_PATH "/dev/input/event3"
// asound
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

#include "../input.h"
#include "../audio.h"
#include "../config.h"

uint8_t audio_read(const uint16_t addr);
void audio_write(const uint16_t addr, const uint8_t val);

#include "peanut_gb.h"
#include "minigb_apu/minigb_apu.h"
#include "MiniFB.h"

static input_map_t gb_keymap[] = {
    {KEY_UP,        VKEY_UP}, 
    {KEY_DOWN,      VKEY_DOWN}, 
    {KEY_LEFT,      VKEY_LEFT}, 
    {KEY_RIGHT,     VKEY_RIGHT},
    {KEY_Z,         VKEY_A}, 
    {KEY_X,         VKEY_B}, 
    {KEY_ENTER,     VKEY_START}, 
    {KEY_BACKSPACE, VKEY_SELECT},
    {KEY_SPACE,     VKEY_EXT_FAST}, 
    {KEY_R,         VKEY_EXT_RESET}, 
    {KEY_P,         VKEY_EXT_PALETTE}, 
    {KEY_ESC,       VKEY_EXT_QUIT}
};

static input_map_t gb_joymap[] = {
    {ABS_HAT0Y,     VKEY_UP},    // up
    {ABS_HAT0Y,     VKEY_DOWN},  // down
    {ABS_HAT0X,     VKEY_LEFT},  // left
    {ABS_HAT0X,     VKEY_RIGHT}, // right

    {ABS_Y,         VKEY_UP}, 
    {ABS_Y,         VKEY_DOWN},
    {ABS_X,         VKEY_LEFT},
    {ABS_X,         VKEY_RIGHT},

    {BTN_A,         VKEY_A}, 
    {BTN_B,         VKEY_B}, 
    {BTN_START,     VKEY_START}, 
    {BTN_SELECT,    VKEY_SELECT},
    {BTN_TR,        VKEY_EXT_QUIT}
};

static struct minigb_apu_ctx apu_ctx;
static snd_pcm_t *pcm_handle = NULL;
static audio_ctx_t *actx = NULL;
static audio_sample_t *audio_buffer = NULL;

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
    if (buf) {
        size_t read_sz = fread(buf, 1, sz, f);
        if (read_sz != (size_t)sz) {
            free(buf);
            buf = NULL;
        }
    }
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
            size_t read_sz = fread(priv.cart_ram, 1, priv.save_size, f);
            if (read_sz != priv.save_size) {
                printf("[Warning] Save file read incomplete.\n");
            }
            fclose(f);
            printf("[System] Loaded save from %s\n", save_path);
        }
    }
}

#if ENABLE_LCD
void lcd_draw_line(struct gb_s *gb, const uint8_t pixels[160], const uint_fast8_t line)
{
    for(unsigned int x = 0; x < LCD_WIDTH; x++)
        priv.fb[line][x] = priv.palette[pixels[x] & 3];
}
#endif
/* input system */
int fast_mode = 1;
int should_quit = 0;

void gb_input_handler(vkey_t vkey, int pressed, void *user_data) {
    struct gb_s *gb = (struct gb_s *)user_data;

    switch (vkey) {
        case VKEY_UP:     if(pressed) gb->direct.joypad &= ~JOYPAD_UP; else gb->direct.joypad |= JOYPAD_UP; break;
        case VKEY_DOWN:   if(pressed) gb->direct.joypad &= ~JOYPAD_DOWN; else gb->direct.joypad |= JOYPAD_DOWN; break;
        case VKEY_LEFT:   if(pressed) gb->direct.joypad &= ~JOYPAD_LEFT; else gb->direct.joypad |= JOYPAD_LEFT; break;
        case VKEY_RIGHT:  if(pressed) gb->direct.joypad &= ~JOYPAD_RIGHT; else gb->direct.joypad |= JOYPAD_RIGHT; break;
        case VKEY_A:      if(pressed) gb->direct.joypad &= ~JOYPAD_A; else gb->direct.joypad |= JOYPAD_A; break;
        case VKEY_B:      if(pressed) gb->direct.joypad &= ~JOYPAD_B; else gb->direct.joypad |= JOYPAD_B; break;
        case VKEY_START:  if(pressed) gb->direct.joypad &= ~JOYPAD_START; else gb->direct.joypad |= JOYPAD_START; break;
        case VKEY_SELECT: if(pressed) gb->direct.joypad &= ~JOYPAD_SELECT; else gb->direct.joypad |= JOYPAD_SELECT; break;

        case VKEY_EXT_FAST:
            fast_mode = pressed ? 3 : 1;
            break;
        case VKEY_EXT_RESET:
            if (pressed) { gb_reset(gb); printf("[System] Reset\n"); }
            break;
        case VKEY_EXT_PALETTE:
            if (pressed) {
                current_palette_idx = (current_palette_idx + 1) % 4;
                memcpy(priv.palette, palettes[current_palette_idx], sizeof(priv.palette));
            }
            break;
        case VKEY_EXT_QUIT:
            if (pressed) should_quit = 1;
            break;
        default: break;
    }
}

int main(int argc, char **argv)
{
    struct gb_s gb;
    enum gb_init_error_e ret;
    config_load("configs/nanoarch.cfg");
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
    
    actx = audio_init(44100, 2, g_config.volume, 0);
    
    if (actx) {
        minigb_apu_audio_init(&apu_ctx);
        audio_buffer = malloc(AUDIO_SAMPLES_TOTAL * sizeof(audio_sample_t));
    } else {
        printf("[Warning] Audio system init failed\n");
    }
    
    input_ctx_t *ictx_kbd = input_init(INPUT_DEVICE_PATH, INPUT_TYPE_KEYBOARD, gb_keymap, 12);
    if (!ictx_kbd) {
        printf("[Input] Warning: Failed to open keyboard at %s\n", INPUT_DEVICE_PATH);
    } else {
        input_set_handler(ictx_kbd, gb_input_handler, &gb);
    }
    
    input_ctx_t *ictx_joy = input_init(g_config.joystick, INPUT_TYPE_JOYSTICK, gb_joymap, 9);
    if (!ictx_joy) {
        printf("[Input] Warning: No joystick detected at %s\n", g_config.joystick);
    } else {
        input_set_handler(ictx_joy, gb_input_handler, &gb);
    }
    printf("[System] Started. Keys: Z=A, X=B, Enter=Start, Back=Select, Space=Fast, R=Reset, P=Palette, ESC=Quit\n");

    while(!should_quit) {
        if (ictx_kbd) input_update(ictx_kbd);
        if (ictx_joy) input_update(ictx_joy);
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
            if (actx && audio_buffer) {
                minigb_apu_audio_callback(&apu_ctx, audio_buffer);
                audio_play_stereo32(actx, (uint32_t *)audio_buffer, AUDIO_SAMPLES_TOTAL / 2);
            } else {
                usleep(16000);
            }

            if (mfb_update(priv.fb, g_config.rotation) < 0) break;
        }
    }

    handle_save(argv[1], 1);
    mfb_close();
    if (ictx_kbd) input_close(ictx_kbd);
    if (ictx_joy) input_close(ictx_joy);
    audio_close(actx);
    if (pcm_handle) snd_pcm_close(pcm_handle);
    if (audio_buffer) free(audio_buffer);
    free(priv.rom);
    free(priv.cart_ram);

    return 0;
}