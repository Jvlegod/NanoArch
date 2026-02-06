#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static void _update_vkey_state(input_ctx_t* ctx, vkey_t vkey, int is_pressed) {
    int old_pressed = (ctx->state & (1 << vkey)) != 0;
    
    if (is_pressed) {
        ctx->state |= (1 << vkey);
    } else {
        ctx->state &= ~(1 << vkey);
    }

    if (old_pressed != is_pressed && ctx->handler) {
        ctx->handler(vkey, is_pressed, ctx->user_data);
    }
}

input_ctx_t* input_init(const char* device_path, input_type_t type, input_map_t* maps, int map_count) {
    int fd = open(device_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return NULL;
    
    input_ctx_t* ctx = calloc(1, sizeof(input_ctx_t));
    if (!ctx) {
        close(fd);
        return NULL;
    }
    
    ctx->fd = fd;
    ctx->type = type;
    ctx->maps = maps;
    ctx->map_count = map_count;
    return ctx;
}

void input_update(input_ctx_t* ctx) {
    struct input_event ev;
    
    while (read(ctx->fd, &ev, sizeof(ev)) > 0) {
        // 1. EV_KEY
        if (ev.type == EV_KEY) {
            if (ev.value == 2) continue; 
            for (int i = 0; i < ctx->map_count; i++) {
                if (ctx->maps[i].linux_code == ev.code) {
                    _update_vkey_state(ctx, ctx->maps[i].vkey, ev.value > 0);
                }
            }
        }
        
        // 2. EV_ABS
        else if (ctx->type == INPUT_TYPE_JOYSTICK && ev.type == EV_ABS) {
            for (int i = 0; i < ctx->map_count; i++) {
                input_map_t *map = &ctx->maps[i];
                if (map->linux_code == ev.code) {
                    int is_pressed = 0;

                    if (map->vkey == VKEY_UP || map->vkey == VKEY_LEFT) {
                        is_pressed = (ev.value < 0);
                    }
                    else if (map->vkey == VKEY_DOWN || map->vkey == VKEY_RIGHT) {
                        is_pressed = (ev.value > 0);
                    }

                    _update_vkey_state(ctx, map->vkey, is_pressed);
                }
            }
        }
    }
}

void input_set_handler(input_ctx_t* ctx, input_handler_t handler, void *user_data) {
    ctx->handler = handler;
    ctx->user_data = user_data;
}

int input_is_pressed(input_ctx_t* ctx, vkey_t vkey) {
    if (!ctx || vkey >= VKEY_MAX) return 0;
    return (ctx->state & (1 << vkey)) != 0;
}

void input_close(input_ctx_t* ctx) {
    if (ctx) {
        if (ctx->fd >= 0) close(ctx->fd);
        free(ctx);
    }
}