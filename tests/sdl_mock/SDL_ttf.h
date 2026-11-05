#pragma once
#include "SDL.h"
struct TTF_Font { int size; };
const char* TTF_GetError();
TTF_Font* TTF_OpenFontRW(SDL_RWops*, int, int);
void TTF_CloseFont(TTF_Font*);
int TTF_SizeUTF8(TTF_Font*, const char*, int*, int*);
void TTF_SetFontStyle(TTF_Font*, int);
#define TTF_STYLE_NORMAL 0x00
#define TTF_STYLE_BOLD 0x01
SDL_Surface* TTF_RenderUTF8_Blended(TTF_Font*, const char*, SDL_Color);
