#pragma once
#include "SDL.h"
constexpr int IMG_INIT_PNG = 0x00000002;
constexpr int IMG_INIT_JPG = 0x00000001;
inline int IMG_Init(int) { return 0; }
inline void IMG_Quit() {}
inline SDL_Surface* IMG_Load(const char*) { return nullptr; }
inline SDL_Surface* IMG_Load_RW(SDL_RWops*, int) { return nullptr; }
inline const char* IMG_GetError() { return "SDL_image unavailable in host mock"; }
