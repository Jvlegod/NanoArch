#define _GNU_SOURCE
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <math.h>

#include "core_manager.h"
#include "render.h"
#include "config.h"
#include "input.h"

#define LOGICAL_W  1280
#define LOGICAL_H  720

#define MENU_X_REL      0.05f
#define PANEL_X_REL     0.35f
#define TITLE_Y_REL     0.10f
#define LIST_Y_REL      0.22f
#define ITEM_H_REL      0.10f 

typedef enum {
    PAGE_MAIN,
    PAGE_CORE_SELECT,
    PAGE_ROM_SELECT,
    PAGE_SETTINGS
} MenuPage;

/* SDL2 Resources */
SDL_Window* win = NULL;
SDL_Renderer* ren = NULL;
SDL_Texture* target_tex = NULL;
SDL_Texture* bg_tex = NULL;
TTF_Font* font = NULL;

typedef struct {
    SDL_Keycode sdl_key;
    vkey_t vkey;
} sdl_input_map_t;

typedef struct {
    MenuPage *page;
    int *cursor;
    bool *quit;
    int *selected_core_idx;
} MgrState;

static sdl_input_map_t key_map[] = {
    {SDLK_UP,     VKEY_UP},     {SDLK_DOWN,   VKEY_DOWN},
    {SDLK_LEFT,   VKEY_LEFT},   {SDLK_RIGHT,  VKEY_RIGHT},
    {SDLK_z,      VKEY_A},      {SDLK_x,      VKEY_B},
    {SDLK_RETURN, VKEY_START},  {SDLK_ESCAPE, VKEY_EXT_QUIT}
};

static input_map_t mgr_joymap[] = {
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
    {BTN_TR,        VKEY_EXT_QUIT}
};

const char* main_menu_items[] = { "Start Game", "Settings", "Quit" };

void mgr_input_handler(vkey_t vkey, int pressed, void *user_data) {
    if (!pressed) return;
    MgrState *state = (MgrState*)user_data;

    int count = 1;
    if (*state->page == PAGE_MAIN) count = sizeof(main_menu_items) / sizeof(main_menu_items[0]);
    else if (*state->page == PAGE_CORE_SELECT) count = CORE_COUNT;
    else if (*state->page == PAGE_ROM_SELECT) count = dynamic_count;

    if (*state->page == PAGE_SETTINGS) {
        if (vkey == VKEY_LEFT) {
            if (g_config.volume >= 10) g_config.volume -= 10;
        } else if (vkey == VKEY_RIGHT) {
            if (g_config.volume <= 90) g_config.volume += 10;
        } else if (vkey == VKEY_B || vkey == VKEY_EXT_QUIT) {
            *state->page = PAGE_MAIN;
            *state->cursor = 1;
            config_save("configs/nanoarch.cfg"); 
        }
        return; 
    }

    switch (vkey) {
        case VKEY_UP:
            *state->cursor = (*state->cursor <= 0) ? (count - 1) : (*state->cursor - 1);
            break;
        case VKEY_DOWN:
            *state->cursor = (*state->cursor >= count - 1) ? 0 : (*state->cursor + 1);
            break;
        case VKEY_EXT_QUIT:
        case VKEY_B:
            if (*state->page == PAGE_ROM_SELECT) {
                *state->page = PAGE_CORE_SELECT;
                *state->cursor = 0;
                clear_dynamic_list();
            } else if (*state->page == PAGE_CORE_SELECT) {
                *state->page = PAGE_MAIN;
                *state->cursor = 0;
            } else {
                *state->quit = true;
            }
            break;
        case VKEY_A:
        case VKEY_START:
            if (*state->page == PAGE_MAIN) {
                if (*state->cursor == 0) {
                    *state->page = PAGE_CORE_SELECT;
                    *state->cursor = 0;
                } else if (*state->cursor == 1) {
                    *state->page = PAGE_SETTINGS;
                } else {
                    *state->quit = true;
                }
            } else if (*state->page == PAGE_CORE_SELECT) {
                *state->selected_core_idx = *state->cursor;
                scan_roms(*state->selected_core_idx);
                *state->page = PAGE_ROM_SELECT;
                *state->cursor = 0;
            } else if (*state->page == PAGE_ROM_SELECT) {
                if (dynamic_list && *state->cursor < dynamic_count) {
                    if (strcmp(dynamic_list[*state->cursor], "<-- Back") == 0) {
                        *state->page = PAGE_CORE_SELECT;
                        *state->cursor = *state->selected_core_idx;
                        clear_dynamic_list();
                    } else {
                        char rom_path[512];
                        snprintf(rom_path, sizeof(rom_path), "roms/%s/%s", 
                                 supported_cores[*state->selected_core_idx].sub_dir, 
                                 dynamic_list[*state->cursor]);
                        launch_external_game(supported_cores[*state->selected_core_idx].binary_path, rom_path);
                    }
                }
            }
            break;
        default: break;
    }
}

bool init_resources() {
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            printf("[SDL] Init failed: %s\n", SDL_GetError());
            return false;
        }
        TTF_Init(); 
        IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    }

    win = SDL_CreateWindow("NanoArch", 
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                           g_config.physical_w, g_config.physical_h, 
                           SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
    if (!win) {
        printf("[SDL] Window creation failed: %s\n", SDL_GetError());
        return false;
    }

    // ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) {
        printf("[SDL] Accelerated renderer failed, fallback to software.\n");
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    }
    
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    target_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, LOGICAL_W, LOGICAL_H);

    font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 48);
    bg_tex = load_texture(ren, "assets/background/background.png");
    
    SDL_ShowCursor(SDL_DISABLE);
    return true;
}

void free_resources() {
    if (target_tex) SDL_DestroyTexture(target_tex);
    if (bg_tex) SDL_DestroyTexture(bg_tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    if (font) TTF_CloseFont(font);
    IMG_Quit(); 
    TTF_Quit(); 
    SDL_Quit();
}

void launch_external_game(const char* exe_path, const char* rom_path) {
    free_resources();
    usleep(100000);
    
    pid_t pid = fork();
    if (pid == 0) {
        execl(exe_path, exe_path, rom_path, NULL);
        perror("Launch failed");
        exit(1); 
    } else {
        int status;
        waitpid(pid, &status, 0);

        config_load("configs/nanoarch.cfg");
        init_resources();
    }
}

int main(int argc, char* argv[]) {
    config_load("configs/nanoarch.cfg");
    if (!init_resources()) return -1;

    MenuPage page = PAGE_MAIN;
    int cursor = 0, selected_core_idx = 0;
    bool quit = false;
    SDL_Event e;
    MgrState mstate = {
        .page = &page,
        .cursor = &cursor,
        .quit = &quit,
        .selected_core_idx = &selected_core_idx
    };
    input_ctx_t *ictx_joy = input_init(g_config.joystick, INPUT_TYPE_JOYSTICK, mgr_joymap, 13);
    if (ictx_joy) {
        input_set_handler(ictx_joy, mgr_input_handler, &mstate);
        printf("[Input] Frontend Joystick initialized: %s\n", g_config.joystick);
    } else {
        printf("[Input] Warning: Joystick not found at %s\n", g_config.joystick);
    }
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                bool pressed = (e.type == SDL_KEYDOWN);
                for (int i = 0; i < sizeof(key_map)/sizeof(key_map[0]); i++) {
                    if (e.key.keysym.sym == key_map[i].sdl_key) {
                        mgr_input_handler(key_map[i].vkey, pressed, &mstate);
                        break;
                    }
                }
            }
        }

        if (ictx_joy) {
            input_update(ictx_joy);
        }

        if (ren) {
            SDL_SetRenderTarget(ren, target_tex);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
            draw_background(ren, bg_tex, LOGICAL_W, LOGICAL_H);

            float menu_x = LOGICAL_W * MENU_X_REL;
            float panel_x = LOGICAL_W * PANEL_X_REL;
            float item_h = LOGICAL_H * ITEM_H_REL;
            SDL_Color main_c = (page == PAGE_MAIN) ? (SDL_Color){255, 255, 255, 255} : (SDL_Color){100, 100, 100, 255};
            for (int i = 0; i < 3; i++) {
                bool sel = (page == PAGE_MAIN && cursor == i);
                draw_menu_text(ren, font, main_menu_items[i], menu_x, (LOGICAL_H * 0.4f) + i * item_h, main_c, sel);
            }
            if (page != PAGE_MAIN) {
                draw_translucent_panel(ren, panel_x, 0, LOGICAL_W - panel_x, LOGICAL_H);
                float content_x = panel_x + 40;

                if (page == PAGE_CORE_SELECT) {
                    draw_menu_text(ren, font, "SELECT SYSTEM", content_x, LOGICAL_H * TITLE_Y_REL, (SDL_Color){200, 200, 255, 255}, false);
                    for (int i = 0; i < CORE_COUNT; i++) {
                        draw_menu_text(ren, font, supported_cores[i].display_name, content_x, (LOGICAL_H * LIST_Y_REL) + i * item_h, (SDL_Color){240, 240, 240, 255}, (cursor == i));
                    }
                }
                else if (page == PAGE_ROM_SELECT) {
                    draw_menu_text(ren, font, "SELECT GAME", content_x, LOGICAL_H * TITLE_Y_REL, (SDL_Color){200, 255, 200, 255}, false);
                    int start_idx = (cursor > 7) ? cursor - 7 : 0;
                    for (int i = 0; i < 8 && (start_idx + i) < dynamic_count; i++) {
                        int idx = start_idx + i;
                        draw_menu_text(ren, font, dynamic_list[idx], content_x, (LOGICAL_H * LIST_Y_REL) + i * (item_h * 0.8f), (SDL_Color){220, 220, 220, 255}, (cursor == idx));
                    }
                }
                else if (page == PAGE_SETTINGS) {
                    draw_menu_text(ren, font, "SETTINGS", content_x, LOGICAL_H * TITLE_Y_REL, (SDL_Color){255, 255, 100, 255}, false);
                    char vol_txt[32]; snprintf(vol_txt, sizeof(vol_txt), "Volume: %d%%", g_config.volume);
                    draw_menu_text(ren, font, vol_txt, content_x, LOGICAL_H * 0.4f, (SDL_Color){255, 255, 255, 255}, true);

                    SDL_Rect bar_bg = { (int)content_x, (int)(LOGICAL_H * 0.5f), (int)(LOGICAL_W * 0.3f), 20 };
                    SDL_Rect bar_fg = { (int)content_x, (int)(LOGICAL_H * 0.5f), (int)((bar_bg.w * g_config.volume) / 100), 20 };
                    SDL_SetRenderDrawColor(ren, 80, 80, 80, 255); SDL_RenderFillRect(ren, &bar_bg);
                    SDL_SetRenderDrawColor(ren, 0, 255, 0, 255); SDL_RenderFillRect(ren, &bar_fg);
                    draw_menu_text(ren, font, "Left/Right to adjust, B to save", content_x, LOGICAL_H * 0.65f, (SDL_Color){150, 150, 150, 255}, false);
                }
            }

            SDL_SetRenderTarget(ren, NULL);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);

            int phys_w = g_config.physical_w;
            int phys_h = g_config.physical_h;

            int visual_w = phys_w;
            int visual_h = phys_h;
            if (g_config.rotation == 90 || g_config.rotation == 270) {
                visual_w = phys_h;
                visual_h = phys_w;
            }

            float scale_x = (float)visual_w / LOGICAL_W;
            float scale_y = (float)visual_h / LOGICAL_H;
            float scale = (scale_x < scale_y) ? scale_x : scale_y;

            int final_w = (int)(LOGICAL_W * scale);
            int final_h = (int)(LOGICAL_H * scale);

            SDL_Rect dst;
            dst.w = final_w;
            dst.h = final_h;

            SDL_Point center;
            center.x = final_w / 2;
            center.y = final_h / 2;

            if (g_config.rotation == 90) {
                dst.x = (phys_w - final_w) / 2; 
                dst.y = (phys_h - final_h) / 2;

                SDL_RenderCopyEx(ren, target_tex, NULL, &dst, (double)g_config.rotation, NULL, SDL_FLIP_NONE);
            } else {
                dst.x = (phys_w - final_w) / 2;
                dst.y = (phys_h - final_h) / 2;
                SDL_RenderCopyEx(ren, target_tex, NULL, &dst, (double)g_config.rotation, NULL, SDL_FLIP_NONE);
            }

            SDL_RenderPresent(ren);
        }
    }

    if (ictx_joy) input_close(ictx_joy);
    free_resources();
    clear_dynamic_list();
    return 0;
}