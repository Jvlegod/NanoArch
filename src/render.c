#include "render.h"

SDL_Texture* load_texture(SDL_Renderer* ren, const char* path) {
    SDL_Texture* tex = IMG_LoadTexture(ren, path);
    if (!tex) {
        printf("Failed to load texture: %s, Error: %s\n", path, IMG_GetError());
    }
    return tex;
}

void draw_background(SDL_Renderer* ren, SDL_Texture* bg_tex, int w, int h) {
    if (bg_tex) {
        // 绘制背景图片，拉伸填充
        SDL_RenderCopy(ren, bg_tex, NULL, NULL);
        
        // 增加半透明黑蒙版 (Alpha=150)
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 150);
        SDL_Rect full_screen = {0, 0, w, h};
        SDL_RenderFillRect(ren, &full_screen);
    } else {
        // 备用背景色
        SDL_SetRenderDrawColor(ren, 30, 30, 35, 255);
        SDL_RenderClear(ren);
    }
}

void draw_text_resizable(SDL_Renderer* ren, TTF_Font* font, const char* text, int x, int y, int target_h, SDL_Color color, bool centered) {
    if (!font || !text || text[0] == '\0') return; // 核心防御

    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);

    float aspect = (float)surf->w / (float)surf->h;
    int target_w = (int)(target_h * aspect);
    SDL_Rect dst = { centered ? x - target_w/2 : x, centered ? y - target_h/2 : y, target_w, target_h };

    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void draw_menu_item(SDL_Renderer* ren, TTF_Font* font, const char* text, int index, int cursor, int w, int h) {
    if (!text) return;
    bool sel = (index == cursor);
    float rect_w = w * 0.5f, rect_h = h * 0.07f;
    float x = w * 0.1f + (sel ? w * 0.02f : 0);
    float y = h * 0.25f + index * (rect_h * 1.5f);

    SDL_Rect r = {(int)x, (int)y, (int)rect_w, (int)rect_h};
    SDL_SetRenderDrawColor(ren, sel ? 0 : 40, sel ? 150 : 40, sel ? 255 : 40, 255);
    SDL_RenderFillRect(ren, &r);

    SDL_Color white = {255, 255, 255, 255};
    draw_text_resizable(ren, font, text, r.x + 20, r.y + (rect_h * 0.2f), rect_h * 0.6f, white, false);
}