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

#include "core_manager.h"
#include "render.h"
#include "config.h"

// 逻辑分辨率
#define LOGICAL_W  1280
#define LOGICAL_H  720

typedef enum { PAGE_MAIN, PAGE_CORE_SELECT, PAGE_ROM_SELECT } MenuPage;
SDL_Window* win = NULL;
SDL_Renderer* ren = NULL;
SDL_Texture* target_tex = NULL;
SDL_Texture* bg_tex = NULL;
TTF_Font* font = NULL;

const char* main_menu_items[] = { "Start Game", "Quit" };

bool init_resources() {
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            printf("[Error] SDL_Init failed: %s\n", SDL_GetError());
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
        printf("[Error] Window creation failed: %s\n", SDL_GetError());
        return false;
    }

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!ren) return false;

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    target_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, LOGICAL_W, LOGICAL_H);

    font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 48);
    if (!font) printf("[Warning] Font not found at assets/fonts/DejaVuSans.ttf!\n");

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
    
    target_tex = NULL;
    bg_tex = NULL;
    ren = NULL;
    win = NULL;
    font = NULL;

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

void launch_external_game(const char* exe_path, const char* rom_path) {
    printf("[System] Launching core: %s\n", exe_path);
    free_resources();

    pid_t pid = fork();
    if (pid < 0) {
        printf("[Error] Fork failed!\n");
        init_resources();
    } 
    else if (pid == 0) {
        execl(exe_path, exe_path, rom_path, NULL);
        perror("[Error] Exec failed");
        exit(1); 
    } 
    else {
        waitpid(pid, NULL, 0);
        init_resources();
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    config_load("configs/nanoarch.cfg");
    if (!init_resources()) return -1;

    MenuPage page = PAGE_MAIN;
    int cursor = 0;
    int selected_core_idx = 0;
    bool quit = false;
    SDL_Event e;

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            
            if (e.type == SDL_KEYDOWN) {
                int count = 0;
                if (page == PAGE_MAIN) count = 2;
                else if (page == PAGE_CORE_SELECT) count = CORE_COUNT;
                else if (page == PAGE_ROM_SELECT) count = dynamic_count;
                if (count == 0) count = 1;

                if (e.key.keysym.sym == SDLK_UP) {
                    cursor--;
                    if (cursor < 0) cursor = count - 1;
                }
                else if (e.key.keysym.sym == SDLK_DOWN) {
                    cursor++;
                    if (cursor >= count) cursor = 0;
                }

                else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    if (page == PAGE_ROM_SELECT) {
                        page = PAGE_CORE_SELECT;
                        cursor = selected_core_idx;
                        clear_dynamic_list();
                    } else if (page == PAGE_CORE_SELECT) {
                        page = PAGE_MAIN;
                        cursor = 0;
                    } else {
                        quit = true;
                    }
                }

                else if (e.key.keysym.sym == SDLK_RETURN) {
                    if (page == PAGE_MAIN) {
                        if (cursor == 0) {
                            page = PAGE_CORE_SELECT;
                            cursor = 0;
                        } else {
                            quit = true;
                        }
                    } 
                    else if (page == PAGE_CORE_SELECT) {
                        selected_core_idx = cursor;
                        scan_roms(selected_core_idx);
                        page = PAGE_ROM_SELECT;
                        cursor = 0;
                    } 
                    else if (page == PAGE_ROM_SELECT) {
                        if (dynamic_list && cursor < dynamic_count) {
                            if (strcmp(dynamic_list[cursor], "<-- Back") == 0) {
                                page = PAGE_CORE_SELECT;
                                cursor = selected_core_idx;
                                clear_dynamic_list();
                            } else {
                                char rom_path[512];
                                snprintf(rom_path, sizeof(rom_path), "roms/%s/%s", 
                                         supported_cores[selected_core_idx].sub_dir, 
                                         dynamic_list[cursor]);
                                launch_external_game(supported_cores[selected_core_idx].binary_path, rom_path);
                            }
                        }
                    }
                }
            }
        }

        if (ren) {
            SDL_SetRenderTarget(ren, target_tex);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
            draw_background(ren, bg_tex, LOGICAL_W, LOGICAL_H);
            {
                SDL_Color menu_color = (page == PAGE_MAIN) 
                                     ? (SDL_Color){255, 255, 255, 255} 
                                     : (SDL_Color){100, 100, 100, 255};
                
                for (int i = 0; i < 2; i++) {
                    bool is_sel = (page == PAGE_MAIN && cursor == i);
                    draw_menu_text(ren, font, main_menu_items[i], 100, 300 + i * 80, menu_color, is_sel);
                }
            }

            if (page == PAGE_CORE_SELECT || page == PAGE_ROM_SELECT) {
                draw_translucent_panel(ren, 500, 0, LOGICAL_W - 500, LOGICAL_H);
                draw_menu_text(ren, font, "Select System:", 540, 50, (SDL_Color){200,200,255,255}, false);
                if (page == PAGE_CORE_SELECT) {
                    for (int i = 0; i < CORE_COUNT; i++) {
                        bool is_sel = (cursor == i);
                        SDL_Color c = {240, 240, 240, 255};
                        draw_menu_text(ren, font, supported_cores[i].display_name, 550, 150 + i * 80, c, is_sel);
                    }
                }
            }

            if (page == PAGE_ROM_SELECT) {
                draw_translucent_panel(ren, 500, 0, LOGICAL_W - 500, LOGICAL_H);

                draw_menu_text(ren, font, "Select Game:", 540, 50, (SDL_Color){200,255,200,255}, false);

                int start_idx = 0;
                int max_visible = 8;
                if (cursor > max_visible - 1) {
                    start_idx = cursor - (max_visible - 1);
                }

                for (int i = 0; i < dynamic_count; i++) {
                    if (i >= start_idx && i < start_idx + max_visible) {
                        bool is_sel = (cursor == i);
                        SDL_Color c = {220, 220, 220, 255};
                        int screen_y = 150 + (i - start_idx) * 60;
                        draw_menu_text(ren, font, dynamic_list[i], 550, screen_y, c, is_sel);
                    }
                }
            }

            SDL_SetRenderTarget(ren, NULL);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);

            SDL_Rect dst_rect = { 0, 0, g_config.physical_w, g_config.physical_h };
            SDL_RenderCopyEx(ren, target_tex, NULL, &dst_rect, (double)g_config.rotation, NULL, SDL_FLIP_NONE);

            SDL_RenderPresent(ren);
        }
    }

    free_resources();
    clear_dynamic_list();
    return 0;
}