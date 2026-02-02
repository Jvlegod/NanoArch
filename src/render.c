#include "render.h"
#include <stdio.h>

SDL_Texture* load_texture(SDL_Renderer* ren, const char* path) {
    SDL_Texture* tex = IMG_LoadTexture(ren, path);
    if (!tex) {
        printf("[Render] Failed to load texture: %s, Error: %s\n", path, IMG_GetError());
    }
    return tex;
}

void draw_background(SDL_Renderer* ren, SDL_Texture* bg_tex, int w, int h) {
    if (bg_tex) {
        SDL_RenderCopy(ren, bg_tex, NULL, NULL);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 100);
        SDL_Rect full_screen = {0, 0, w, h};
        SDL_RenderFillRect(ren, &full_screen);
    } else {
        SDL_SetRenderDrawColor(ren, 30, 30, 35, 255);
        SDL_RenderClear(ren);
    }
}

void draw_translucent_panel(SDL_Renderer* ren, int x, int y, int w, int h) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
    
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(ren, &rect);
}

void draw_menu_text(SDL_Renderer* ren, TTF_Font* font, const char* text, int x, int y, SDL_Color color, bool is_selected) {
    if (!font || !text || text[0] == '\0') return;

    if (is_selected) {
        color = (SDL_Color){255, 220, 0, 255};
    }

    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    if (tex) {
        SDL_Rect dst = { x, y, surf->w, surf->h };
        SDL_RenderCopy(ren, tex, NULL, &dst);
        
        SDL_DestroyTexture(tex);
    }
    
    SDL_FreeSurface(surf);
}