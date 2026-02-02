#ifndef RENDER_H
#define RENDER_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <stdbool.h>

SDL_Texture* load_texture(SDL_Renderer* ren, const char* path);
void draw_background(SDL_Renderer* ren, SDL_Texture* bg_tex, int w, int h);
void draw_translucent_panel(SDL_Renderer* ren, int x, int y, int w, int h);
void draw_menu_text(SDL_Renderer* ren, TTF_Font* font, const char* text, int x, int y, SDL_Color color, bool is_selected);

#endif