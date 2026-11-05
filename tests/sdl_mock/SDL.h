#pragma once
#include <cstdint>

using Uint8 = std::uint8_t;
using Uint32 = std::uint32_t;
#define SDL_PIXELFORMAT_RGBA32 1
struct SDL_Color { Uint8 r, g, b, a; };
struct SDL_Rect { int x, y, w, h; };
struct SDL_Renderer {};
struct SDL_Texture {};
struct SDL_PixelFormat {};
struct SDL_Surface {
    int w{};
    int h{};
    int pitch{};
    void* pixels{};
    SDL_PixelFormat* format{};
};
struct SDL_RWops {};
constexpr int SDL_BLENDMODE_BLEND = 1;

int SDL_RenderSetLogicalSize(SDL_Renderer*, int, int);
int SDL_SetRenderDrawBlendMode(SDL_Renderer*, int);
const char* SDL_GetError();
SDL_RWops* SDL_RWFromConstMem(const void*, int);
SDL_Surface* SDL_CreateRGBSurfaceWithFormatFrom(void*, int, int, int, int, Uint32);
SDL_Surface* SDL_CreateRGBSurfaceWithFormat(Uint32, int, int, int, Uint32);
Uint32 SDL_MapRGBA(SDL_PixelFormat*, Uint8, Uint8, Uint8, Uint8);
void SDL_SetRenderDrawColor(SDL_Renderer*, Uint8, Uint8, Uint8, Uint8);
int SDL_RenderClear(SDL_Renderer*);
int SDL_RenderDrawLine(SDL_Renderer*, int, int, int, int);
int SDL_RenderFillRect(SDL_Renderer*, const SDL_Rect*);
int SDL_RenderDrawRect(SDL_Renderer*, const SDL_Rect*);
int SDL_RenderCopy(SDL_Renderer*, SDL_Texture*, const SDL_Rect*, const SDL_Rect*);
int SDL_QueryTexture(SDL_Texture*, Uint32*, int*, int*, int*);
void SDL_RenderPresent(SDL_Renderer*);
SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer*, SDL_Surface*);
void SDL_DestroyTexture(SDL_Texture*);
void SDL_FreeSurface(SDL_Surface*);
