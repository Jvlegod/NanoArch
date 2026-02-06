#ifndef UNIFIED_INPUT_H
#define UNIFIED_INPUT_H

#include <stdint.h>
#include <linux/input.h>

typedef enum {
    VKEY_UP, VKEY_DOWN, VKEY_LEFT, VKEY_RIGHT,
    VKEY_A, VKEY_B, VKEY_START, VKEY_SELECT,
    VKEY_EXT_FAST, VKEY_EXT_RESET, VKEY_EXT_PALETTE, VKEY_EXT_QUIT,
    VKEY_MAX
} vkey_t;

typedef enum {
    INPUT_TYPE_KEYBOARD,
    INPUT_TYPE_JOYSTICK,
} input_type_t;

typedef struct {
    int linux_code;
    vkey_t vkey;
} input_map_t;

typedef void (*input_handler_t)(vkey_t vkey, int pressed, void *user_data);

typedef struct {
    int fd;
    input_type_t type;
    uint32_t state;
    input_map_t *maps;
    int map_count;
    input_handler_t handler;
    void *user_data;
} input_ctx_t;

input_ctx_t* input_init(const char* device_path, input_type_t type, input_map_t* maps, int map_count);
void input_set_handler(input_ctx_t* ctx, input_handler_t handler, void *user_data);
void input_update(input_ctx_t* ctx);
int  input_is_pressed(input_ctx_t* ctx, vkey_t vkey);
void input_close(input_ctx_t* ctx);

#endif