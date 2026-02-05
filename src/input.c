#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

input_ctx_t* input_init(const char* device_path, input_map_t* maps, int map_count) {
    int fd = open(device_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return NULL;
    input_ctx_t* ctx = calloc(1, sizeof(input_ctx_t));
    ctx->fd = fd;
    ctx->maps = maps;
    ctx->map_count = map_count;
    return ctx;
}

void input_update(input_ctx_t* ctx) {
    struct input_event ev;
    while (read(ctx->fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY) {
            if (ev.value == 2) continue;

            for (int i = 0; i < ctx->map_count; i++) {
                if (ctx->maps[i].linux_code == ev.code) {
                    int is_pressed = (ev.value > 0);

                    if (is_pressed) ctx->state |= (1 << ctx->maps[i].vkey);
                    else            ctx->state &= ~(1 << ctx->maps[i].vkey);

                    if (ctx->handler) {
                        ctx->handler(ctx->maps[i].vkey, is_pressed, ctx->user_data);
                    }
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
    return (ctx->state & (1 << vkey)) != 0;
}

void input_close(input_ctx_t* ctx) {
    if (ctx) {
        close(ctx->fd);
        free(ctx);
    }
}