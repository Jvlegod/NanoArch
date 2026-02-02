#ifndef RENDER_H
#define RENDER_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <stdbool.h>

SDL_Texture* load_texture(SDL_Renderer* ren, const char* path);
void draw_background(SDL_Renderer* ren, SDL_Texture* bg_tex, int w, int h);
void draw_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color);
void draw_text_resizable(SDL_Renderer* ren, TTF_Font* font, const char* text, int x, int y, int target_h, SDL_Color color, bool centered);
void draw_menu_item(SDL_Renderer* ren, TTF_Font* font, const char* text, int index, int cursor, int w, int h);

#endif