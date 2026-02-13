#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int fb_fd = -1;
static unsigned char *fb_mem = NULL;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static long screensize = 0;

int mfb_open(const char* title, int width, int height)
{
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        perror("Error opening /dev/fb0");
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Error getting finfo");
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Error getting vinfo");
        return -1;
    }

    screensize = finfo.smem_len; 
    fb_mem = (unsigned char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    
    if (fb_mem == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }

    memset(fb_mem, 0, screensize);

    printf("FB Initialized: %dx%d, %dbpp, line_len: %d\n", 
            vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, finfo.line_length);

    return 1;
}

int mfb_update(void* buffer, int rotation)
{
    if (fb_mem == NULL || buffer == NULL) return -1;

    int screen_w = vinfo.xres;
    int screen_h = vinfo.yres;
    int bpp = vinfo.bits_per_pixel / 8;

    const int GB_W = 160;
    const int GB_H = 144;
    const int SCALE = 3; 

    uint32_t *src = (uint32_t *)buffer;

    int logic_out_w, logic_out_h;
    if (rotation == 90 || rotation == 270) {
        logic_out_w = GB_H * SCALE;
        logic_out_h = GB_W * SCALE;
    } else {
        logic_out_w = GB_W * SCALE;
        logic_out_h = GB_H * SCALE;
    }

    int start_x = (screen_w - logic_out_w) / 2;
    int start_y = (screen_h - logic_out_h) / 2;

    for (int y = 0; y < GB_H; y++) {
        for (int x = 0; x < GB_W; x++) {
            uint32_t pixel = src[y * GB_W + x];

            uint16_t color16 = (bpp == 2) ? 
                (((pixel >> 19) << 11) | (((pixel >> 10) & 0x3F) << 5) | ((pixel >> 3) & 0x1F)) : 0;

            for (int i = 0; i < SCALE; i++) {
                for (int j = 0; j < SCALE; j++) {
                    int phys_x, phys_y;

                    switch (rotation) {
                        case 90:
                            phys_x = (GB_H - 1 - y) * SCALE + i + start_x;
                            phys_y = x * SCALE + j + start_y;
                            break;
                        case 270:
                            phys_x = y * SCALE + i + start_x;
                            phys_y = (GB_W - 1 - x) * SCALE + j + start_y;
                            break;
                        case 180:
                            phys_x = (GB_W - 1 - x) * SCALE + i + start_x;
                            phys_y = (GB_H - 1 - y) * SCALE + j + start_y;
                            break;
                        case 0:
                        default:
                            phys_x = x * SCALE + i + start_x;
                            phys_y = y * SCALE + j + start_y;
                            break;
                    }

                    if (phys_x >= 0 && phys_x < screen_w && phys_y >= 0 && phys_y < screen_h) {
                        unsigned char *dst = fb_mem + (phys_y * finfo.line_length) + (phys_x * bpp);
                        if (bpp == 4) *(uint32_t *)dst = pixel;
                        else if (bpp == 2) *(uint16_t *)dst = color16;
                    }
                }
            }
        }
    }
    return 0;
}

void mfb_close(void) {
    if (fb_mem && fb_mem != MAP_FAILED) munmap(fb_mem, screensize);
    if (fb_fd >= 0) close(fb_fd);
    fb_mem = NULL;
}