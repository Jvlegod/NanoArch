#ifndef CORE_API_H
#define CORE_API_H

#include <stdint.h>

typedef struct {
    const char* name;
    void (*init)(void);
    void (*load_game)(const char* path);
    void (*run_frame)(uint32_t* buffer);
    void (*input)(uint32_t keys);
    void (*cleanup)(void);
} NanoCore;

typedef NanoCore* (*get_core_ptr)(void);

#endif