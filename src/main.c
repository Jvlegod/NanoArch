#define _GNU_SOURCE
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_api.h"
#include "core_manager.h"
#include "render.h" 
#include "config.h"

#define LOGICAL_W  1280
#define LOGICAL_H  720

typedef enum { APP_MENU, APP_RUNNING } AppState;
typedef enum { PAGE_MAIN, PAGE_CORES, PAGE_ROM_SELECT } MenuPage;

typedef NanoCore* (*get_core_func_t)(void);

NanoCore* current_core = NULL;
void* core_handle = NULL;

SDL_Texture* game_texture = NULL;
SDL_Texture* target_tex = NULL;
uint32_t* video_buffer = NULL;

const char* main_menu_items[] = { "Select Core", "Quit" };

void unload_core() {
    if (!current_core && !core_handle) return;

    printf("[System] Unloading core...\n");
    SDL_CloseAudio();

    if (current_core && current_core->cleanup) {
        current_core->cleanup();
    }

    if (core_handle) {
        dlclose(core_handle);
        core_handle = NULL; 
    }

    current_core = NULL;
    printf("[System] Core unloaded.\n");
}

bool load_core_library(const char* so_path) {
    unload_core();

    core_handle = dlopen(so_path, RTLD_LAZY);
    if (!core_handle) {
        printf("Failed to load core: %s. Error: %s\n", so_path, dlerror());
        return false;
    }

    get_core_func_t get_core = (get_core_func_t)dlsym(core_handle, "get_core");
    if (!get_core) {
        printf("Symbol 'get_core' not found in %s\n", so_path);
        dlclose(core_handle); core_handle = NULL;
        return false;
    }

    current_core = get_core();
    if (current_core->init) current_core->init();
    printf("[System] Loaded Core: %s\n", current_core->name);
    return true;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    config_load("configs/nanoarch.cfg");

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

    printf("[Init] Window Size: %dx%d\n", g_config.physical_w, g_config.physical_h);
    SDL_Window* win = SDL_CreateWindow("NanoArch", 
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                       g_config.physical_w, g_config.physical_h, 
                                       SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
    if (!win) {
        printf("[Error] CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        printf("[Error] CreateRenderer failed: %s\n", SDL_GetError());
        printf("[System] Fallback to Software Renderer...\n");
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
        if (!ren) {
            printf("[Fatal] Software Renderer also failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(win);
            return -1;
        }
    }

    target_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, LOGICAL_W, LOGICAL_H);

    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 48);
    if (!font) { printf("Font Error: %s\n", TTF_GetError()); return -1; }

    SDL_Texture* bg_tex = load_texture(ren, "assets/background/background.png"); 

    game_texture = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 240);
    video_buffer = malloc(256 * 240 * sizeof(uint32_t));

    AppState appState = APP_MENU;
    MenuPage page = PAGE_MAIN;
    int cursor = 0;
    int selected_core_idx = -1;
    bool quit = false;
    SDL_Event e;

    while (!quit) {

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    if (appState == APP_RUNNING) {
                        unload_core();
                        appState = APP_MENU;
                        break;
                    } else {
                        if (page == PAGE_ROM_SELECT) {
                            page = PAGE_CORES; 
                            cursor = 0; 
                        }
                        else if (page == PAGE_CORES) {
                            page = PAGE_MAIN; 
                            cursor = 0; 
                        }
                    }
                }

                if (appState == APP_MENU) {
                    int count = 1;
                    if (page == PAGE_MAIN) count = 2;
                    else if (page == PAGE_CORES) count = CORE_COUNT + 1;
                    else if (page == PAGE_ROM_SELECT) count = dynamic_count;
                    if (count == 0) count = 1;

                    if (e.key.keysym.sym == SDLK_UP)   cursor = (cursor - 1 + count) % count;
                    if (e.key.keysym.sym == SDLK_DOWN) cursor = (cursor + 1) % count;
                    
                    if (e.key.keysym.sym == SDLK_RETURN) {
                        if (page == PAGE_MAIN) {
                            if (cursor == 0) { page = PAGE_CORES; cursor = 0; }
                            else if (cursor == 1) quit = true;
                        }
                        else if (page == PAGE_CORES) {
                            if (cursor == CORE_COUNT) { page = PAGE_MAIN; cursor = 0; }
                            else {
                                selected_core_idx = cursor;
                                char so_path[256];
                                snprintf(so_path, sizeof(so_path), "cores/%s.so", supported_cores[selected_core_idx].sub_dir);
                                if (load_core_library(so_path)) {
                                    scan_roms(selected_core_idx);
                                    page = PAGE_ROM_SELECT;
                                    cursor = 0;
                                }
                            }
                        }
                        else if (page == PAGE_ROM_SELECT) {
                             if (dynamic_list && strcmp(dynamic_list[cursor], "<-- Back") == 0) {
                                page = PAGE_CORES; cursor = 0;
                            } else if (dynamic_list) {
                                char rom_path[512];
                                snprintf(rom_path, sizeof(rom_path), "roms/%s/%s", 
                                         supported_cores[selected_core_idx].sub_dir, dynamic_list[cursor]);
                                
                                if (current_core && current_core->load_game) {
                                    current_core->load_game(rom_path);
                                    appState = APP_RUNNING;
                                }
                            }
                        }
                    }
                }
            }
        }

        SDL_SetRenderTarget(ren, target_tex);
        SDL_SetRenderDrawColor(ren, 30, 30, 35, 255);
        SDL_RenderClear(ren);

        if (appState == APP_RUNNING && current_core) {
            // if (current_core->input) current_core->input(keys);
            current_core->run_frame(video_buffer); // now we just run frame use emulator
            // SDL_UpdateTexture(game_texture, NULL, video_buffer, 256 * sizeof(uint32_t));
            // SDL_RenderCopy(ren, game_texture, NULL, NULL);
        } 
        else {
            if (bg_tex) draw_background(ren, bg_tex, LOGICAL_W, LOGICAL_H);

            int count = 0;
            if (page == PAGE_MAIN) count = 2;
            else if (page == PAGE_CORES) count = CORE_COUNT + 1;
            else if (page == PAGE_ROM_SELECT) count = dynamic_count;
            if (count == 0) count = 1;

            for (int i = 0; i < count; i++) {
                const char* label = "Unknown";
                if (page == PAGE_MAIN) label = main_menu_items[i];
                else if (page == PAGE_CORES) label = (i == CORE_COUNT) ? "<-- Back" : supported_cores[i].display_name;
                else if (page == PAGE_ROM_SELECT) label = (dynamic_list && i < dynamic_count) ? dynamic_list[i] : "Err";

                draw_menu_item(ren, font, label, i, cursor, LOGICAL_W, LOGICAL_H);
            }
        }

        SDL_SetRenderTarget(ren, NULL);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        SDL_Rect dst_rect = { 0, 0, g_config.physical_w, g_config.physical_h };
        SDL_RenderCopyEx(ren, target_tex, NULL, &dst_rect, (double)g_config.rotation, NULL, SDL_FLIP_NONE);
        SDL_RenderPresent(ren);
    }

    unload_core();
    if (video_buffer) free(video_buffer);
    if (game_texture) SDL_DestroyTexture(game_texture);
    if (target_tex) SDL_DestroyTexture(target_tex);
    if (bg_tex) SDL_DestroyTexture(bg_tex);
    
    clear_dynamic_list();
    TTF_CloseFont(font);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}