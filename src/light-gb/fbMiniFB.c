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

/* 初始化 Framebuffer */
int mfb_open(const char* title, int width, int height)
{
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        perror("Error opening /dev/fb0");
        return -1;
    }

    // 获取显存固定参数（如 line_length）
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Error getting finfo");
        return -1;
    }

    // 获取显存可变参数（如分辨率、位深）
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Error getting vinfo");
        return -1;
    }

    // 计算显存总大小并映射
    screensize = finfo.smem_len; 
    fb_mem = (unsigned char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    
    if (fb_mem == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }

    // 清屏（黑色）
    memset(fb_mem, 0, screensize);

    printf("FB Initialized: %dx%d, %dbpp, line_len: %d\n", 
            vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, finfo.line_length);

    return 1;
}

int mfb_update(void* buffer)
{
    if (fb_mem == NULL || buffer == NULL) return -1;

    int screen_w = vinfo.xres;
    int screen_h = vinfo.yres;
    int bpp = vinfo.bits_per_pixel / 8;
    
    // Peanut-GB 原始尺寸
    const int GB_W = 160;
    const int GB_H = 144;
    // 放大后的尺寸
    const int SCALE = 3; 
    const int OUT_W = GB_W * SCALE;
    const int OUT_H = GB_H * SCALE;

    // 重新计算 320x288 居中所需的偏移量
    int start_x = (screen_w - OUT_W) / 2;
    int start_y = (screen_h - OUT_H) / 2;

    // 越界检查（如果屏幕小于 320x288，则强制不放大或切边）
    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;

    uint32_t *src = (uint32_t *)buffer;

    

    for (int y = 0; y < GB_H; y++) {
        // 垂直方向：每一行原始像素要重复写 SCALE 次
        for (int i = 0; i < SCALE; i++) {
            unsigned char *dst_line = fb_mem + (y * SCALE + i + start_y) * finfo.line_length + (start_x * bpp);
            
            if (bpp == 4) {
                // 32位屏幕 (RGBA8888)
                uint32_t *dst_ptr = (uint32_t *)dst_line;
                for (int x = 0; x < GB_W; x++) {
                    uint32_t pixel = src[y * GB_W + x];
                    // 水平方向：每一个像素重复写 SCALE 次
                    for (int j = 0; j < SCALE; j++) {
                        dst_ptr[x * SCALE + j] = pixel;
                    }
                }
            } 
            else if (bpp == 2) {
                // 16位屏幕 (RGB565)
                uint16_t *dst_ptr = (uint16_t *)dst_line;
                for (int x = 0; x < GB_W; x++) {
                    uint32_t p = src[y * GB_W + x];
                    // 预先转换颜色，减少内层循环计算量
                    uint16_t color = ((p >> 19) << 11) | (((p >> 10) & 0x3F) << 5) | (p >> 3);
                    for (int j = 0; j < SCALE; j++) {
                        dst_ptr[x * SCALE + j] = color;
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