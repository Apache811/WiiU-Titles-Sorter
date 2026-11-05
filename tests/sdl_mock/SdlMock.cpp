#include "SDL.h"
#include "SDL_ttf.h"
#include "SdlMock.hpp"

#include <algorithm>
#include <cstring>

namespace { SdlMockStats stats; }
SdlMockStats& GetSdlMockStats() { return stats; }
void ResetSdlMockStats() { stats = {}; }

int SDL_RenderSetLogicalSize(SDL_Renderer*, int w, int h) { stats.logicalWidth = w; stats.logicalHeight = h; return 0; }
int SDL_SetRenderDrawBlendMode(SDL_Renderer*, int) { return 0; }
const char* SDL_GetError() { return "mock SDL error"; }
SDL_RWops* SDL_RWFromConstMem(const void*, int) { return new SDL_RWops{}; }
void SDL_SetRenderDrawColor(SDL_Renderer*, Uint8, Uint8, Uint8, Uint8) {}
int SDL_RenderClear(SDL_Renderer*) { ++stats.clearCalls; return 0; }
int SDL_RenderDrawLine(SDL_Renderer*, int, int, int, int) { ++stats.lineCalls; return 0; }
int SDL_RenderFillRect(SDL_Renderer*, const SDL_Rect*) { return 0; }
int SDL_RenderDrawRect(SDL_Renderer*, const SDL_Rect*) { return 0; }
int SDL_RenderCopy(SDL_Renderer*, SDL_Texture*, const SDL_Rect*, const SDL_Rect*) { ++stats.copyCalls; return 0; }
int SDL_QueryTexture(SDL_Texture*, Uint32*, int*, int* w, int* h) { if (w) *w = 128; if (h) *h = 128; return 0; }
void SDL_RenderPresent(SDL_Renderer*) { ++stats.presentCalls; }
SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer*, SDL_Surface*) { ++stats.createdTextures; return new SDL_Texture{}; }
void SDL_DestroyTexture(SDL_Texture* texture) { if (texture) { ++stats.destroyedTextures; delete texture; } }
void SDL_FreeSurface(SDL_Surface* surface) {
    if (!surface) return;
    delete[] static_cast<std::uint8_t*>(surface->pixels);
    delete surface->format;
    delete surface;
}
const char* TTF_GetError() { return "mock SDL_ttf error"; }
TTF_Font* TTF_OpenFontRW(SDL_RWops* rw, int freesrc, int size) { if (freesrc) delete rw; return new TTF_Font{size}; }
void TTF_CloseFont(TTF_Font* font) { delete font; }
void TTF_SetFontStyle(TTF_Font*, int) {}
int TTF_SizeUTF8(TTF_Font* font, const char* text, int* w, int* h) {
    if (!font || !text || !w || !h) return -1;
    *w = static_cast<int>(std::strlen(text)) * std::max(1, font->size / 2); *h = font->size; return 0;
}
SDL_Surface* TTF_RenderUTF8_Blended(TTF_Font* font, const char* text, SDL_Color) {
    if (!font || !text) return nullptr;
    return new SDL_Surface{static_cast<int>(std::strlen(text)) * std::max(1, font->size / 2), font->size};
}

SDL_Surface* SDL_CreateRGBSurfaceWithFormatFrom(void*, int, int, int, int, Uint32) { return nullptr; }
SDL_Surface* SDL_CreateRGBSurfaceWithFormat(Uint32, int w, int h, int depth, Uint32) {
    if (w <= 0 || h <= 0 || depth != 32) return nullptr;
    auto* surface = new SDL_Surface{};
    surface->w = w;
    surface->h = h;
    surface->pitch = w * 4;
    surface->pixels = new std::uint8_t[static_cast<std::size_t>(surface->pitch) * static_cast<std::size_t>(h)]{};
    surface->format = new SDL_PixelFormat{};
    return surface;
}
Uint32 SDL_MapRGBA(SDL_PixelFormat*, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    return (static_cast<Uint32>(r) << 24U) | (static_cast<Uint32>(g) << 16U) |
           (static_cast<Uint32>(b) << 8U) | static_cast<Uint32>(a);
}
