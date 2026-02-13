#ifndef CONFIG_H
#define CONFIG_H
#include <stdbool.h>

typedef struct {
    int physical_w;
    int physical_h;
    int rotation;
    int volume; // 0-100
    bool keep_aspect;
    char joystick[128];
} AppConfig;

extern AppConfig g_config;
void config_load(const char* filename);

#endif