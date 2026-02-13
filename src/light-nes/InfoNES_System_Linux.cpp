/*===================================================================*/
/* */
/* InfoNES_System_Linux.cpp : Linux specific File                   */
/* */
/* Refactored for Embedded Linux (Framebuffer + ALSA + Evdev)       */
/* */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/* Include files                                                    */
/*-------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <alsa/asoundlib.h>

#include "InfoNES.h"
#include "InfoNES_System.h"
#include "InfoNES_pAPU.h"

extern "C" {
    #include "../input.h"
    #include "../audio.h"
    #include "../config.h"
}

/*-------------------------------------------------------------------*/
/* Configuration                                                    */
/*-------------------------------------------------------------------*/

#define INPUT_DEVICE_PATH "/dev/input/event0" 

static input_map_t nes_keymap[] = {
    {KEY_UP,        VKEY_UP}, 
    {KEY_DOWN,      VKEY_DOWN}, 
    {KEY_LEFT,      VKEY_LEFT}, 
    {KEY_RIGHT,     VKEY_RIGHT},
    {KEY_Z,         VKEY_A}, 
    {KEY_X,         VKEY_B}, 
    {KEY_ENTER,     VKEY_START}, 
    {KEY_BACKSPACE, VKEY_SELECT},
    {KEY_ESC,       VKEY_EXT_QUIT}
};

static input_map_t nes_joymap[] = {
    {ABS_HAT0Y,     VKEY_UP},    
    {ABS_HAT0Y,     VKEY_DOWN},  
    {ABS_HAT0X,     VKEY_LEFT},  
    {ABS_HAT0X,     VKEY_RIGHT}, 

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

#define NES_A       (1 << 0)
#define NES_B       (1 << 1)
#define NES_SELECT  (1 << 2)
#define NES_START   (1 << 3)
#define NES_UP      (1 << 4)
#define NES_DOWN    (1 << 5)
#define NES_LEFT    (1 << 6)
#define NES_RIGHT   (1 << 7)

#define TRUE 1
#define FALSE 0

/*-------------------------------------------------------------------*/
/* Global Variables                                                 */
/*-------------------------------------------------------------------*/

// Sound
#define NES_SAMPLE_RATE 44100
#define AUDIO_VOLUME 100
static audio_ctx_t *nes_actx = NULL;

// Framebuffer
#define LCD_SCALE 2
static int fb_fd = -1;
static unsigned char *fb_mem;
static int px_width;
static int line_width;
static int screen_width;
static int lcd_width;
static int lcd_height;
static struct fb_var_screeninfo var;
// accerelated palette
static uint32_t color_LUT32[65536]; 

static int *zoom_x_tab;
static int *zoom_y_tab;

// Input
static input_ctx_t *nes_ictx = NULL;
static input_ctx_t *nes_ijoy = NULL;

// ROM Info
char szRomName[256];
char szSaveName[256];
int nSRAM_SaveFlag;

// Thread & State
pthread_t emulation_tid;
int bThread = FALSE;
volatile int g_bLoop = TRUE;

// Pad State
DWORD dwKeyPad1 = 0;
DWORD dwKeyPad2 = 0;
DWORD dwKeySystem = 0;


/*-------------------------------------------------------------------*/
/* Function prototypes                                              */
/*-------------------------------------------------------------------*/

void *emulation_thread(void *args);
void start_application(char *filename);
int LoadSRAM();
int SaveSRAM();
int input_init();
void process_input();

/*-------------------------------------------------------------------*/
/* Display Functions                                                */
/*-------------------------------------------------------------------*/
void Init_NeoPalette() {
    for (int i = 0; i < 65536; i++) {
        uint16_t c = (uint16_t)i;
        uint8_t r, g, b;

        r = ((c >> 10) & 0x1F) * 255 / 31;
        g = ((c >>  5) & 0x1F) * 255 / 31;
        b = ((c >>  0) & 0x1F) * 255 / 31;

        uint32_t pix = 0;
        pix |= ((uint32_t)r << var.red.offset);
        pix |= ((uint32_t)g << var.green.offset);
        pix |= ((uint32_t)b << var.blue.offset);

        if (var.transp.length)
            pix |= (((1u << var.transp.length) - 1u) << var.transp.offset);
        else
            pix |= (0xFF << 24);

        color_LUT32[i] = pix;
    }
}

static inline uint32_t pack_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t pix = 0;
    uint32_t rv = r >> (8 - var.red.length);
    uint32_t gv = g >> (8 - var.green.length);
    uint32_t bv = b >> (8 - var.blue.length);

    pix |= (rv << var.red.offset);
    pix |= (gv << var.green.offset);
    pix |= (bv << var.blue.offset);

    if (var.transp.length)
        pix |= (((1u << var.transp.length) - 1u) << var.transp.offset);

    return pix;
}

static int lcd_fb_init()
{
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        printf("[Display] Can't open /dev/fb0\n");
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) == -1) {
        close(fb_fd);
        printf("[Display] Can't ioctl /dev/fb0\n");
        return -1;
    }

    px_width = var.bits_per_pixel / 8;
    line_width = var.xres * px_width;
    screen_width = var.yres * line_width;
    lcd_width = var.xres;
    lcd_height = var.yres;

    printf("[Display] FB: %dx%d, %dbpp\n", lcd_width, lcd_height, var.bits_per_pixel);

    fb_mem = (unsigned char *)mmap(NULL, screen_width, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_mem == (void *)-1) {
        close(fb_fd);
        printf("[Display] Can't mmap /dev/fb0\n");
        return -1;
    }

    memset(fb_mem, 0, screen_width);
    return 0;
}

int make_zoom_tab()
{
    int i;
    zoom_x_tab = (int *)malloc(sizeof(int) * lcd_width);
    if (!zoom_x_tab) return -1;
    
    for (i = 0; i < lcd_width; i++) {
        zoom_x_tab[i] = i * NES_DISP_WIDTH / lcd_width;
    }

    zoom_y_tab = (int *)malloc(sizeof(int) * lcd_height);
    if (!zoom_y_tab) return -1;

    for (i = 0; i < lcd_height; i++) {
        zoom_y_tab[i] = i * NES_DISP_HEIGHT / lcd_height;
    }
    return 1;
}

/*-------------------------------------------------------------------*/
/* Input Functions                                                   */
/*-------------------------------------------------------------------*/

void nes_input_handler(vkey_t vkey, int pressed, void *user_data) {
    switch (vkey) {
        case VKEY_UP:     if(pressed) dwKeyPad1 |= NES_UP;     else dwKeyPad1 &= ~NES_UP; break;
        case VKEY_DOWN:   if(pressed) dwKeyPad1 |= NES_DOWN;   else dwKeyPad1 &= ~NES_DOWN; break;
        case VKEY_LEFT:   if(pressed) dwKeyPad1 |= NES_LEFT;   else dwKeyPad1 &= ~NES_LEFT; break;
        case VKEY_RIGHT:  if(pressed) dwKeyPad1 |= NES_RIGHT;  else dwKeyPad1 &= ~NES_RIGHT; break;
        case VKEY_A:      if(pressed) dwKeyPad1 |= NES_A;      else dwKeyPad1 &= ~NES_A; break;
        case VKEY_B:      if(pressed) dwKeyPad1 |= NES_B;      else dwKeyPad1 &= ~NES_B; break;
        case VKEY_START:  if(pressed) dwKeyPad1 |= NES_START;  else dwKeyPad1 &= ~NES_START; break;
        case VKEY_SELECT: if(pressed) dwKeyPad1 |= NES_SELECT; else dwKeyPad1 &= ~NES_SELECT; break;
        
        case VKEY_EXT_QUIT:
            if(pressed) {
                printf("[System] Quit requested via input handler.\n");
                g_bLoop = FALSE;
            }
            break;
        default: break;
    }
}

/*-------------------------------------------------------------------*/
/* Main Entry Point                                                 */
/*-------------------------------------------------------------------*/

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s <rom.nes>\n", argv[0]);
        return 0;
    }

    // 1. Initialize Display
    if (lcd_fb_init() < 0) return -1;
    if (make_zoom_tab() < 0) return -1;

    Init_NeoPalette();
    // 2. Initialize Input
    nes_ictx = input_init(INPUT_DEVICE_PATH, INPUT_TYPE_KEYBOARD, nes_keymap, sizeof(nes_keymap)/sizeof(input_map_t));
    if (nes_ictx) {
        input_set_handler(nes_ictx, nes_input_handler, NULL);
    }

    nes_ijoy = input_init(g_config.joystick, INPUT_TYPE_JOYSTICK, nes_joymap, sizeof(nes_joymap)/sizeof(input_map_t));
    if (nes_ijoy) {
        input_set_handler(nes_ijoy, nes_input_handler, NULL);
        printf("[Input] Joystick initialized at %s\n", g_config.joystick);
    }

    // 3. Start Game
    start_application(argv[1]);

    // 4. Main Loop (Input Polling)
    printf("[System] NES Started. Z=A, X=B, Enter=Start, Back=Select, ESC=Quit\n");
    while (g_bLoop) {
        if (nes_ictx) input_update(nes_ictx);
        if (nes_ijoy) input_update(nes_ijoy);
        usleep(1000); // 1ms sleep to prevent 100% CPU usage in input thread
    }

    // 5. Cleanup
    bThread = FALSE;
    if (emulation_tid) {
        pthread_join(emulation_tid, NULL);
    }

    InfoNES_SoundClose();
    if (nes_ictx) input_close(nes_ictx);
    if (nes_ijoy) input_close(nes_ijoy);

    if (fb_mem && fb_mem != (void*)-1) munmap(fb_mem, screen_width);
    if (fb_fd >= 0) close(fb_fd);
    if (zoom_x_tab) free(zoom_x_tab);
    if (zoom_y_tab) free(zoom_y_tab);

    return 0;
}

/*-------------------------------------------------------------------*/
/* Emulation Thread                                                 */
/*-------------------------------------------------------------------*/

void *emulation_thread(void *args)
{
    InfoNES_Main();
    g_bLoop = FALSE; // Stop main loop if emulator exits
    return NULL;
}

void start_application(char *filename)
{
    strcpy(szRomName, filename);

    if (InfoNES_Load(szRomName) == 0) {
        LoadSRAM();
        bThread = TRUE;
        pthread_create(&emulation_tid, NULL, emulation_thread, NULL);
    } else {
        printf("[System] Failed to load ROM: %s\n", filename);
        g_bLoop = FALSE;
    }
}

/*-------------------------------------------------------------------*/
/* SRAM Functions                                                   */
/*-------------------------------------------------------------------*/

int LoadSRAM()
{
    FILE *fp;
    unsigned char pSrcBuf[SRAM_SIZE];
    unsigned char chData, chTag;
    int nRunLen, nDecoded, nDecLen, nIdx;

    nSRAM_SaveFlag = 0;
    if (!ROM_SRAM) return 0;
    nSRAM_SaveFlag = 1;

    strcpy(szSaveName, szRomName);
    strcpy(strrchr(szSaveName, '.') + 1, "srm");

    fp = fopen(szSaveName, "rb");
    if (fp == NULL) return -1;

    fread(pSrcBuf, SRAM_SIZE, 1, fp);
    fclose(fp);

    // Simple RLE Decompression
    nDecoded = 0;
    nDecLen = 0;
    chTag = pSrcBuf[nDecoded++];

    while (nDecLen < 8192) {
        chData = pSrcBuf[nDecoded++];
        if (chData == chTag) {
            chData = pSrcBuf[nDecoded++];
            nRunLen = pSrcBuf[nDecoded++];
            for (nIdx = 0; nIdx < nRunLen + 1; ++nIdx)
                SRAM[nDecLen++] = chData;
        } else {
            SRAM[nDecLen++] = chData;
        }
    }
    printf("[System] SRAM Loaded\n");
    return 0;
}

int SaveSRAM()
{
    FILE *fp;
    int nUsedTable[256];
    unsigned char chData, chPrevData, chTag;
    int nIdx, nEncoded, nEncLen, nRunLen;
    unsigned char pDstBuf[SRAM_SIZE];

    if (!nSRAM_SaveFlag) return 0;

    // Simple RLE Compression
    memset(nUsedTable, 0, sizeof nUsedTable);
    for (nIdx = 0; nIdx < SRAM_SIZE; ++nIdx) ++nUsedTable[SRAM[nIdx++]];
    for (nIdx = 1, chTag = 0; nIdx < 256; ++nIdx) {
        if (nUsedTable[nIdx] < nUsedTable[chTag]) chTag = nIdx;
    }

    nEncoded = 0;
    nEncLen = 0;
    nRunLen = 1;
    pDstBuf[nEncLen++] = chTag;
    chPrevData = SRAM[nEncoded++];

    while (nEncoded < SRAM_SIZE && nEncLen < SRAM_SIZE - 133) {
        chData = SRAM[nEncoded++];
        if (chPrevData == chData && nRunLen < 256)
            ++nRunLen;
        else {
            if (nRunLen >= 4 || chPrevData == chTag) {
                pDstBuf[nEncLen++] = chTag;
                pDstBuf[nEncLen++] = chPrevData;
                pDstBuf[nEncLen++] = nRunLen - 1;
            } else {
                for (nIdx = 0; nIdx < nRunLen; ++nIdx)
                    pDstBuf[nEncLen++] = chPrevData;
            }
            chPrevData = chData;
            nRunLen = 1;
        }
    }
    // Flush remaining
    if (nRunLen >= 4 || chPrevData == chTag) {
        pDstBuf[nEncLen++] = chTag;
        pDstBuf[nEncLen++] = chPrevData;
        pDstBuf[nEncLen++] = nRunLen - 1;
    } else {
        for (nIdx = 0; nIdx < nRunLen; ++nIdx)
            pDstBuf[nEncLen++] = chPrevData;
    }

    fp = fopen(szSaveName, "wb");
    if (fp == NULL) return -1;
    fwrite(pDstBuf, nEncLen, 1, fp);
    fclose(fp);
    printf("[System] SRAM Saved\n");
    return 0;
}

/*-------------------------------------------------------------------*/
/* InfoNES Interface Implementation                                 */
/*-------------------------------------------------------------------*/

int InfoNES_Menu()
{
    if (bThread == FALSE) return -1;
    return 0;
}

int InfoNES_ReadRom(const char *pszFileName)
{
    FILE *fp;
    fp = fopen(pszFileName, "rb");
    if (fp == NULL) return -1;

    fread(&NesHeader, sizeof NesHeader, 1, fp);
    if (memcmp(NesHeader.byID, "NES\x1a", 4) != 0) {
        fclose(fp);
        return -1;
    }

    memset(SRAM, 0, SRAM_SIZE);
    if (NesHeader.byInfo1 & 4) {
        fread(&SRAM[0x1000], 512, 1, fp);
    }

    ROM = (BYTE *)malloc(NesHeader.byRomSize * 0x4000);
    fread(ROM, 0x4000, NesHeader.byRomSize, fp);

    if (NesHeader.byVRomSize > 0) {
        VROM = (BYTE *)malloc(NesHeader.byVRomSize * 0x2000);
        fread(VROM, 0x2000, NesHeader.byVRomSize, fp);
    }

    fclose(fp);
    return 0;
}

void InfoNES_ReleaseRom()
{
    if (ROM) { free(ROM); ROM = NULL; }
    if (VROM) { free(VROM); VROM = NULL; }
}

void *InfoNES_MemoryCopy(void *dest, const void *src, int count)
{
    memcpy(dest, src, count);
    return dest;
}

void *InfoNES_MemorySet(void *dest, int c, int count)
{
    memset(dest, c, count);
    return dest;
}

void InfoNES_LoadFrame() {
    if (!fb_mem) return;

    int rotation = g_config.rotation;
    int scale = (rotation == 90 || rotation == 270) ? 2 : LCD_SCALE;
    int logic_w = (rotation == 90 || rotation == 270) ? (240 * scale) : (256 * scale);
    int logic_h = (rotation == 90 || rotation == 270) ? (256 * scale) : (240 * scale);

    int start_x = (lcd_width - logic_w) / 2;
    int start_y = (lcd_height - logic_h) / 2;
    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;

    for (int y = 0; y < 240; y++) {
        WORD *line_src = &WorkFrame[y * NES_DISP_WIDTH];
        for (int x = 0; x < 256; x++) {
            uint32_t color = color_LUT32[line_src[x]];
            int px, py;

            switch (rotation) {
                case 90:
                    px = (239 - y) * scale + start_x;
                    py = x * scale + start_y;
                    break;
                case 270:
                    px = y * scale + start_x;
                    py = (255 - x) * scale + start_y;
                    break;
                case 180:
                    px = (255 - x) * scale + start_x;
                    py = (239 - y) * scale + start_y;
                    break;
                default:
                    px = x * scale + start_x;
                    py = y * scale + start_y;
                    break;
            }

            for (int sy = 0; sy < scale; sy++) {
                uint32_t *p_line = (uint32_t *)(fb_mem + (py + sy) * line_width + px * px_width);
                for (int sx = 0; sx < scale; sx++) {
                    p_line[sx] = color;
                }
            }
        }
    }
}

void InfoNES_PadState(DWORD *pdwPad1, DWORD *pdwPad2, DWORD *pdwSystem)
{
    *pdwPad1 = dwKeyPad1;
    *pdwPad2 = dwKeyPad2;
    *pdwSystem = dwKeySystem;
}

void InfoNES_SoundInit(void) {}

int InfoNES_SoundOpen(int samples_per_sync, int sample_rate)
{
    config_load("configs/nanoarch.cfg");
    nes_actx = audio_init(sample_rate, 1, g_config.volume, 1);
    
    if (nes_actx == NULL) {
        printf("[Audio] Failed to initialize unified audio system\n");
        return 0;
    }
    return 1;
}

void InfoNES_SoundClose(void)
{
    if (nes_actx) {
        audio_close(nes_actx);
        nes_actx = NULL;
    }
}

void InfoNES_SoundOutput(int samples, BYTE *wave1, BYTE *wave2, BYTE *wave3, BYTE *wave4, BYTE *wave5) {
    if (!nes_actx) return;

    static uint8_t pcmBuf[8192]; 
    if (samples > 8192) samples = 8192;

    for (int i = 0; i < samples; i++) {
        pcmBuf[i] = (uint8_t)((wave1[i] + wave2[i] + wave3[i] + wave4[i] + wave5[i]) / 5);
    }
    audio_play_mono8(nes_actx, pcmBuf, samples);
}

void InfoNES_Wait() {}

void InfoNES_MessageBox(const char *pszMsg, ...)
{
    printf("[InfoNES] Msg: %s\n", pszMsg);
}

/* Palette data (Standard NES Palette) */
WORD NesPalette[64] =
{
    0x39ce, 0x1071, 0x0015, 0x2013, 0x440e, 0x5402, 0x5000, 0x3c20,
    0x20a0, 0x0100, 0x0140, 0x00e2, 0x0ceb, 0x0000, 0x0000, 0x0000,
    0x5ef7, 0x01dd, 0x10fd, 0x401e, 0x5c17, 0x700b, 0x6ca0, 0x6521,
    0x45c0, 0x0240, 0x02a0, 0x0247, 0x0211, 0x0000, 0x0000, 0x0000,
    0x7fff, 0x1eff, 0x2e5f, 0x223f, 0x79ff, 0x7dd6, 0x7dcc, 0x7e67,
    0x7ae7, 0x4342, 0x2769, 0x2ff3, 0x03bb, 0x0000, 0x0000, 0x0000,
    0x7fff, 0x579f, 0x635f, 0x6b3f, 0x7f1f, 0x7f1b, 0x7ef6, 0x7f75,
    0x7f94, 0x73f4, 0x57d7, 0x5bf9, 0x4ffe, 0x0000, 0x0000, 0x0000
};