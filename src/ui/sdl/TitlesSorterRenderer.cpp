#include "ui/TitlesSorterRenderer.hpp"
#include <SDL_image.h>
#if defined(__WIIU__)
#include "Log.hpp"
#include <coreinit/memory.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <cstdint>
#include <vector>

#if defined(__WIIU__)
#include "ZL_png.h"
#include "ZR_png.h"
#include "unknown_png.h"
#include "lock_png.h"
#include "folder_blue_png.h"
#include "folder_green_png.h"
#include "folder_yellow_png.h"
#include "folder_orange_png.h"
#include "folder_red_png.h"
#include "folder_rose_png.h"
#include "folder_purple_png.h"
#include "folder_grey_png.h"
#endif

namespace titles_sorter {
namespace {

SDL_Surface* TryLoadIconPath(const std::string& path) {
    if (path.empty()) return nullptr;
    if (SDL_Surface* surface = IMG_Load(path.c_str())) return surface;

    // WUT applications may expose the same CafeOS volume through either the
    // device-prefixed devoptab path or the /vol path used by CafeOS APIs.
    const auto tryPath = [](const std::string& candidate) -> SDL_Surface* {
        return candidate.empty() ? nullptr : IMG_Load(candidate.c_str());
    };
    if (path.rfind("storage_mlc01:/", 0U) == 0U) {
        const std::string suffix = path.substr(std::string("storage_mlc01:").size());
        if (SDL_Surface* surface = tryPath("/vol/storage_mlc01" + suffix)) return surface;
        if (SDL_Surface* surface = tryPath("fs:/vol/storage_mlc01" + suffix)) return surface;
    } else if (path.rfind("/vol/storage_mlc01/", 0U) == 0U) {
        const std::string suffix = path.substr(std::string("/vol/storage_mlc01").size());
        if (SDL_Surface* surface = tryPath("storage_mlc01:" + suffix)) return surface;
        if (SDL_Surface* surface = tryPath("fs:/vol/storage_mlc01" + suffix)) return surface;
    } else if (path.rfind("storage_odd03:/", 0U) == 0U) {
        const std::string suffix = path.substr(std::string("storage_odd03:").size());
        if (SDL_Surface* surface = tryPath("/vol/storage_odd03" + suffix)) return surface;
        if (SDL_Surface* surface = tryPath("fs:/vol/storage_odd03" + suffix)) return surface;
    }
    return nullptr;
}

struct TgaRgba {
    Uint8 r{};
    Uint8 g{};
    Uint8 b{};
    Uint8 a{255U};
};

bool ReadTgaPixel(const std::vector<std::uint8_t>& bytes, std::size_t& offset,
                  std::uint8_t depth, bool greyscale, bool hasAlpha,
                  TgaRgba& pixel) noexcept {
    const std::size_t bytesPerPixel = static_cast<std::size_t>(depth / 8U);
    if (bytesPerPixel == 0U || offset > bytes.size() ||
        bytesPerPixel > bytes.size() - offset) return false;

    if (greyscale) {
        pixel.r = bytes[offset];
        pixel.g = bytes[offset];
        pixel.b = bytes[offset];
        pixel.a = depth == 16U ? bytes[offset + 1U] : 255U;
    } else if (depth == 16U) {
        const std::uint16_t value = static_cast<std::uint16_t>(
            bytes[offset] | (static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U));
        pixel.r = static_cast<Uint8>(((value >> 10U) & 0x1FU) * 255U / 31U);
        pixel.g = static_cast<Uint8>(((value >> 5U) & 0x1FU) * 255U / 31U);
        pixel.b = static_cast<Uint8>((value & 0x1FU) * 255U / 31U);
        pixel.a = !hasAlpha || (value & 0x8000U) != 0U ? 255U : 0U;
    } else {
        pixel.b = bytes[offset];
        pixel.g = bytes[offset + 1U];
        pixel.r = bytes[offset + 2U];
        pixel.a = depth == 32U ? bytes[offset + 3U] : 255U;
    }
    offset += bytesPerPixel;
    return true;
}

SDL_Surface* DecodeTgaSurface(const std::vector<std::uint8_t>& bytes) {
    // SDL_image handles the usual TGA variants. This bounded fallback handles
    // Wii U bundle TGA encodings that SDL_image builds may reject (including RLE).
    if (bytes.size() < 18U || bytes[1] != 0U) return nullptr;
    const std::uint8_t imageType = bytes[2];
    const bool rle = imageType == 10U || imageType == 11U;
    const bool greyscale = imageType == 3U || imageType == 11U;
    if (!(imageType == 2U || imageType == 3U || imageType == 10U || imageType == 11U))
        return nullptr;

    const std::uint16_t width = static_cast<std::uint16_t>(
        bytes[12] | (static_cast<std::uint16_t>(bytes[13]) << 8U));
    const std::uint16_t height = static_cast<std::uint16_t>(
        bytes[14] | (static_cast<std::uint16_t>(bytes[15]) << 8U));
    const std::uint8_t depth = bytes[16];
    const bool hasAlpha = (bytes[17] & 0x0FU) != 0U;
    if (width == 0U || height == 0U || width > 4096U || height > 4096U ||
        static_cast<std::size_t>(width) * height > 1024U * 1024U) return nullptr;
    if (greyscale ? (depth != 8U && depth != 16U)
                  : (depth != 16U && depth != 24U && depth != 32U)) return nullptr;

    std::size_t offset = 18U + bytes[0];
    if (offset > bytes.size()) return nullptr;
    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, static_cast<int>(width), static_cast<int>(height), 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr || surface->pixels == nullptr || surface->format == nullptr) {
        if (surface != nullptr) SDL_FreeSurface(surface);
        return nullptr;
    }

    const bool topOrigin = (bytes[17] & 0x20U) != 0U;
    const bool rightOrigin = (bytes[17] & 0x10U) != 0U;
    const auto storePixel = [&](std::size_t fileIndex, Uint32 mapped) {
        const std::size_t fileY = fileIndex / width;
        const std::size_t fileX = fileIndex % width;
        const std::size_t y = topOrigin ? fileY : static_cast<std::size_t>(height) - 1U - fileY;
        const std::size_t x = rightOrigin ? static_cast<std::size_t>(width) - 1U - fileX : fileX;
        auto* row = reinterpret_cast<Uint32*>(
            static_cast<std::uint8_t*>(surface->pixels) + y * static_cast<std::size_t>(surface->pitch));
        row[x] = mapped;
    };

    std::size_t outputIndex = 0U;
    while (outputIndex < pixelCount) {
        std::size_t runCount = 1U;
        bool repeated = false;
        if (rle) {
            if (offset >= bytes.size()) { SDL_FreeSurface(surface); return nullptr; }
            const std::uint8_t packet = bytes[offset++];
            runCount = static_cast<std::size_t>(packet & 0x7FU) + 1U;
            repeated = (packet & 0x80U) != 0U;
            if (runCount > pixelCount - outputIndex) { SDL_FreeSurface(surface); return nullptr; }
        }

        if (repeated) {
            TgaRgba pixel;
            if (!ReadTgaPixel(bytes, offset, depth, greyscale, hasAlpha, pixel)) {
                SDL_FreeSurface(surface);
                return nullptr;
            }
            const Uint32 mapped = SDL_MapRGBA(surface->format, pixel.r, pixel.g, pixel.b, pixel.a);
            for (std::size_t i = 0; i < runCount; ++i) storePixel(outputIndex++, mapped);
        } else {
            for (std::size_t i = 0; i < runCount; ++i) {
                TgaRgba pixel;
                if (!ReadTgaPixel(bytes, offset, depth, greyscale, hasAlpha, pixel)) {
                    SDL_FreeSurface(surface);
                    return nullptr;
                }
                storePixel(outputIndex++, SDL_MapRGBA(surface->format, pixel.r, pixel.g, pixel.b, pixel.a));
            }
        }
    }
    return surface;
}

SDL_Surface* LoadTitleIconSurface(const PreviewItem& item) {
    if (!item.iconPath.empty() && item.iconPath != "__disc_channel__") {
        if (SDL_Surface* surface = TryLoadIconPath(item.iconPath)) return surface;
    }

    if (item.kind == ItemKind::Disc) {
        // WiiUBrew lists 00050010-10055000 as the nominal Disc icon title.
        static constexpr const char* discPaths[] = {
            "storage_mlc01:/sys/title/00050010/10055000/meta/iconTex.tga",
            "/vol/storage_mlc01/sys/title/00050010/10055000/meta/iconTex.tga",
            "storage_odd03:/meta/iconTex.tga",
            "/vol/storage_odd03/meta/iconTex.tga",
            "fs:/vol/storage_odd03/meta/iconTex.tga",
            "storage_odd03:/vol/content/meta/iconTex.tga",
            "/vol/storage_odd03/vol/content/meta/iconTex.tga",
            "storage_odd01:/meta/iconTex.tga",
            "/vol/storage_odd01/meta/iconTex.tga",
            "fs:/vol/storage_odd01/meta/iconTex.tga"
        };
        for (const char* path : discPaths) {
            if (SDL_Surface* surface = TryLoadIconPath(path)) return surface;
        }
    } else if (item.kind == ItemKind::Vwii) {
        // Regional Wii Menu IDs: Japan, North America, Europe.
        static constexpr std::uint32_t regionalIds[] = {0x10004000U, 0x10004001U, 0x10004009U, 0x1000400AU};
        for (const std::uint32_t lowerId : regionalIds) {
            char path[112]{};
            std::snprintf(path, sizeof(path),
                "storage_mlc01:/sys/title/00050010/%08X/meta/iconTex.tga", lowerId);
            if (SDL_Surface* surface = TryLoadIconPath(path)) return surface;
        }
    }
    return nullptr;
}

} // namespace

namespace {
constexpr int kTopBarHeight = 140;
constexpr int kBottomBarHeight = 90;
constexpr std::size_t kCacheLimit = 128U;
constexpr float kPi = 3.14159265358979323846f;

std::string MakeTextKey(const std::string& text, int size, SDL_Color color) {
    std::string key;
    key.reserve(text.size() + 24U);
    key.append(std::to_string(size)).push_back('|');
    key.append(std::to_string(color.r)).push_back(',');
    key.append(std::to_string(color.g)).push_back(',');
    key.append(std::to_string(color.b)).push_back(',');
    key.append(std::to_string(color.a)).push_back('|');
    key.append(text);
    return key;
}

std::string FitUtf8(TTF_Font* font, const std::string& text, int maxWidth) {
    if (font == nullptr || maxWidth <= 0 || text.empty()) return text;
    int width = 0;
    int height = 0;
    if (TTF_SizeUTF8(font, text.c_str(), &width, &height) == 0 && width <= maxWidth) return text;

    std::string candidate = text;
    while (!candidate.empty()) {
        std::size_t start = candidate.size() - 1U;
        while (start > 0U && (static_cast<unsigned char>(candidate[start]) & 0xC0U) == 0x80U) --start;
        candidate.resize(start);
        const std::string fitted = candidate + "...";
        if (TTF_SizeUTF8(font, fitted.c_str(), &width, &height) == 0 && width <= maxWidth) return fitted;
    }
    return "...";
}

SDL_Color Interpolate(SDL_Color a, SDL_Color b, float t) {
    const auto channel = [t](Uint8 from, Uint8 to) {
        return static_cast<Uint8>(static_cast<float>(from) +
            (static_cast<float>(to) - static_cast<float>(from)) * t);
    };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a)};
}
} // namespace

TitlesSorterRenderer::~TitlesSorterRenderer() { Shutdown(); }

bool TitlesSorterRenderer::Initialize(SDL_Renderer* renderer, const void* fontData,
                                      std::size_t fontSize, std::string& error) {
    error.clear();
    Shutdown();
    if (renderer == nullptr || fontData == nullptr || fontSize == 0U ||
        fontSize > static_cast<std::size_t>(0x7FFFFFFF)) {
        error = "Renderer or embedded font data is invalid.";
        return false;
    }
    renderer_ = renderer;
    if (SDL_RenderSetLogicalSize(renderer_, LogicalWidth, LogicalHeight) != 0) {
        error = std::string("Unable to set logical render size: ") + SDL_GetError();
        Shutdown();
        return false;
    }
    if (SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND) != 0) {
        error = std::string("Unable to enable renderer alpha blending: ") + SDL_GetError();
        Shutdown();
        return false;
    }

    // TTF_OpenFontRW takes ownership of the RWops when freesrc is non-zero.
    SDL_RWops* stream = SDL_RWFromConstMem(fontData, static_cast<int>(fontSize));
    if (stream == nullptr) {
        error = std::string("Unable to create font stream: ") + SDL_GetError();
        Shutdown();
        return false;
    }
    // Keep a private font copy so additional point sizes can be opened lazily.
    fontData_.assign(static_cast<const std::uint8_t*>(fontData),
                     static_cast<const std::uint8_t*>(fontData) + fontSize);
    TTF_Font* baseFont = TTF_OpenFontRW(stream, 1, 34);
    if (baseFont == nullptr) {
        error = std::string("Unable to load embedded font: ") + TTF_GetError();
        Shutdown();
        return false;
    }
    fonts_.push_back({34, baseFont});
#if defined(__WIIU__)
    // Match Titles Info: decode embedded PNG data through SDL_image rather
    // than depending on libpng's optional simplified png_image API.
    const auto loadPngTexture = [this](const unsigned char* bytes, unsigned int byteCount) -> SDL_Texture* {
        if (bytes == nullptr || byteCount == 0U ||
            byteCount > static_cast<unsigned int>(0x7FFFFFFF)) return nullptr;
        SDL_RWops* stream = SDL_RWFromConstMem(bytes, static_cast<int>(byteCount));
        if (stream == nullptr) return nullptr;
        SDL_Surface* surface = IMG_Load_RW(stream, 1);
        if (surface == nullptr) return nullptr;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_FreeSurface(surface);
        return texture;
    };
    zlTexture_ = loadPngTexture(ZL_png, ZL_png_size);
    zrTexture_ = loadPngTexture(ZR_png, ZR_png_size);
    unknownTexture_ = loadPngTexture(unknown_png, unknown_png_size);
    lockTexture_ = loadPngTexture(lock_png, lock_png_size);

    // Folder PNGs are pre-scaled by tools/prepare_assets.py during the build.
    // Loading them directly avoids decoding full-resolution source images and
    // allocating temporary scaling surfaces on the Wii U.
    folderTextures_[static_cast<std::size_t>(FolderColor::Blue)] =
        loadPngTexture(folder_blue_png, folder_blue_png_size);
    folderTextures_[static_cast<std::size_t>(FolderColor::Green)] =
        loadPngTexture(folder_green_png, folder_green_png_size);
    folderTextures_[static_cast<std::size_t>(FolderColor::Yellow)] =
        loadPngTexture(folder_yellow_png, folder_yellow_png_size);
    folderTextures_[static_cast<std::size_t>(FolderColor::Orange)] =
        loadPngTexture(folder_orange_png, folder_orange_png_size);
    folderTextures_[static_cast<std::size_t>(FolderColor::Red)] =
        loadPngTexture(folder_red_png, folder_red_png_size);
    folderTextures_[static_cast<std::size_t>(FolderColor::Rose)] =
        loadPngTexture(folder_rose_png, folder_rose_png_size);
    folderTextures_[static_cast<std::size_t>(FolderColor::Purple)] =
        loadPngTexture(folder_purple_png, folder_purple_png_size);
    folderTextures_[static_cast<std::size_t>(FolderColor::Grey)] =
        loadPngTexture(folder_grey_png, folder_grey_png_size);

    (void)OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD, 0, &iconFontData_, &iconFontSize_);
#endif
    initialized_ = true;
    return true;
}

void TitlesSorterRenderer::Shutdown() noexcept {
    for (auto& entry : textCache_) {
        if (entry.second.texture != nullptr) SDL_DestroyTexture(entry.second.texture);
    }
    textCache_.clear();
    cacheOrder_.clear();
    for (auto& entry : glyphCache_) if (entry.second.texture != nullptr) SDL_DestroyTexture(entry.second.texture);
    glyphCache_.clear();
    for (auto& entry : iconFonts_) if (entry.second != nullptr) TTF_CloseFont(entry.second);
    iconFonts_.clear();
    iconFontData_ = nullptr;
    iconFontSize_ = 0U;
    if (zlTexture_ != nullptr) { SDL_DestroyTexture(zlTexture_); zlTexture_ = nullptr; }
    if (zrTexture_ != nullptr) { SDL_DestroyTexture(zrTexture_); zrTexture_ = nullptr; }
    if (unknownTexture_ != nullptr) { SDL_DestroyTexture(unknownTexture_); unknownTexture_ = nullptr; }
    if (lockTexture_ != nullptr) { SDL_DestroyTexture(lockTexture_); lockTexture_ = nullptr; }
    for (SDL_Texture*& texture : folderTextures_) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
    }
    for (auto& entry : iconTextures_) if (entry.second != nullptr) SDL_DestroyTexture(entry.second);
    iconTextures_.clear();
    for (FontEntry& entry : fonts_) {
        if (entry.font != nullptr) TTF_CloseFont(entry.font);
    }
    fonts_.clear();
    fontData_.clear();
    renderer_ = nullptr;
    initialized_ = false;
}

TTF_Font* TitlesSorterRenderer::GetFont(int size) {
    for (const FontEntry& entry : fonts_) if (entry.size == size) return entry.font;
    if (fontData_.empty() || size <= 0) return nullptr;
    SDL_RWops* stream = SDL_RWFromConstMem(fontData_.data(), static_cast<int>(fontData_.size()));
    if (stream == nullptr) return nullptr;
    TTF_Font* font = TTF_OpenFontRW(stream, 1, size);
    if (font == nullptr) return nullptr;
    fonts_.push_back({size, font});
    return font;
}

TitlesSorterRenderer::CachedTexture* TitlesSorterRenderer::GetTextTexture(
    const std::string& text, int size, SDL_Color color) {
    if (text.empty() || renderer_ == nullptr) return nullptr;
    const std::string key = MakeTextKey(text, size, color);
    auto found = textCache_.find(key);
    if (found != textCache_.end()) return &found->second;

    TTF_Font* font = GetFont(size);
    if (font == nullptr) return nullptr;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (surface == nullptr) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    const int width = surface->w;
    const int height = surface->h;
    SDL_FreeSurface(surface);
    if (texture == nullptr) return nullptr;

    if (textCache_.size() >= static_cast<std::size_t>(kCacheLimit) && !cacheOrder_.empty()) {
        const std::string& oldest = cacheOrder_.front();
        auto old = textCache_.find(oldest);
        if (old != textCache_.end()) {
            if (old->second.texture != nullptr) SDL_DestroyTexture(old->second.texture);
            textCache_.erase(old);
        }
        cacheOrder_.pop_front();
    }
    cacheOrder_.push_back(key);
    auto inserted = textCache_.emplace(key, CachedTexture{texture, width, height});
    return &inserted.first->second;
}

void TitlesSorterRenderer::DrawText(const std::string& text, int x, int y, int size,
                                    SDL_Color color, int maxWidth) {
    std::string fitted = text;
    if (maxWidth > 0) fitted = FitUtf8(GetFont(size), text, maxWidth);
    CachedTexture* cached = GetTextTexture(fitted, size, color);
    if (cached == nullptr || cached->texture == nullptr) return;
    SDL_Rect destination{x, y, cached->width, cached->height};
    if (maxWidth > 0 && destination.w > maxWidth) destination.w = maxWidth;
    SDL_RenderCopy(renderer_, cached->texture, nullptr, &destination);
}

int TitlesSorterRenderer::DrawGlyph(const char* glyph, int x, int centerY, int size, SDL_Color color) {
    if (glyph == nullptr || !*glyph || iconFontData_ == nullptr || iconFontSize_ == 0U) return 0;
    std::string key = std::to_string(size) + ":" + glyph;
    auto found = glyphCache_.find(key);
    if (found == glyphCache_.end()) {
        TTF_Font* font = nullptr;
        const auto fontFound = iconFonts_.find(size);
        if (fontFound != iconFonts_.end()) font = fontFound->second;
        else {
            SDL_RWops* stream = SDL_RWFromConstMem(iconFontData_, static_cast<int>(iconFontSize_));
            if (stream != nullptr) font = TTF_OpenFontRW(stream, 1, size);
            if (font != nullptr) iconFonts_.emplace(size, font);
        }
        if (font == nullptr) return 0;
        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, glyph, color);
        if (surface == nullptr) return 0;
        CachedTexture entry;
        entry.texture = SDL_CreateTextureFromSurface(renderer_, surface);
        entry.width = surface->w; entry.height = surface->h;
        SDL_FreeSurface(surface);
        if (entry.texture == nullptr) return 0;
        found = glyphCache_.emplace(key, entry).first;
    }
    const CachedTexture& entry = found->second;
    SDL_Rect destination{x, centerY - entry.height / 2, entry.width, entry.height};
    SDL_RenderCopy(renderer_, entry.texture, nullptr, &destination);
    return entry.width;
}

void TitlesSorterRenderer::DrawGradient(const SDL_Rect& rect, SDL_Color top, SDL_Color bottom) {
    if (rect.h <= 0 || rect.w <= 0) return;
    for (int y = 0; y < rect.h; ++y) {
        const float t = rect.h > 1 ? static_cast<float>(y) / static_cast<float>(rect.h - 1) : 0.0f;
        const SDL_Color color = Interpolate(top, bottom, t);
        SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
        SDL_RenderDrawLine(renderer_, rect.x, rect.y + y, rect.x + rect.w - 1, rect.y + y);
    }
}

void TitlesSorterRenderer::DrawRoundedRect(const SDL_Rect& rect, int radius, SDL_Color color) {
    if (rect.w <= 0 || rect.h <= 0) return;
    radius = std::max(0, std::min(radius, std::min(rect.w, rect.h) / 2));
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int y = 0; y < rect.h; ++y) {
        const int dy = y < radius ? radius - y :
            (y >= rect.h - radius ? y - (rect.h - radius - 1) : 0);
        int inset = 0;
        if (dy > 0) {
            const float inside = static_cast<float>(radius * radius - dy * dy);
            inset = radius - static_cast<int>(std::sqrt(std::max(0.0f, inside)));
        }
        SDL_RenderDrawLine(renderer_, rect.x + inset, rect.y + y,
                           rect.x + rect.w - 1 - inset, rect.y + y);
    }
}

void TitlesSorterRenderer::DrawRoundedBorder(const SDL_Rect& rect, int radius, int thickness,
                                              SDL_Color color, SDL_Color innerColor) {
    DrawRoundedRect(rect, radius, color);
    const SDL_Rect inner{rect.x + thickness, rect.y + thickness,
                         rect.w - thickness * 2, rect.h - thickness * 2};
    DrawRoundedRect(inner, std::max(0, radius - thickness), innerColor);
}

void TitlesSorterRenderer::DrawBackground() {
    SDL_SetRenderDrawColor(renderer_, UiPalette::Background.r, UiPalette::Background.g,
                           UiPalette::Background.b, UiPalette::Background.a);
    SDL_RenderClear(renderer_);
}

void TitlesSorterRenderer::DrawTopBar(const UiFrame& frame) {
    DrawGradient({0, 0, LogicalWidth, kTopBarHeight}, UiPalette::TopBarStart, UiPalette::TopBarEnd);
    DrawText("Titles Sorter", 64, 39, 54, UiPalette::White);

    // Keep the right edge inset equal to the title's left inset.
    constexpr int rightInset = 64;
    constexpr int buttonWidth = 88;
    constexpr int buttonHeight = 62;
    constexpr int tabWidth = 220;
    constexpr int tabHeight = 78;
    // The extra tab expands the group to the left: PREVIEW/OPTIONS/ZL move
    // left while the ZR button keeps its previous fixed X coordinate.
    constexpr int gap = 40;
    constexpr int totalWidth = buttonWidth + gap + tabWidth + gap + tabWidth + gap + tabWidth + gap + buttonWidth;
    const int startX = LogicalWidth - rightInset - totalWidth;
    const int y = (kTopBarHeight - tabHeight) / 2;
    if (zlTexture_ != nullptr) {
        const SDL_Rect destination{startX, (kTopBarHeight-buttonHeight)/2, buttonWidth, buttonHeight};
        SDL_RenderCopy(renderer_, zlTexture_, nullptr, &destination);
    } else DrawText("ZL", startX + 24, y + 24, 25, UiPalette::White);

    const auto drawTab = [this](const char* label, int x, bool active) {
        const SDL_Rect rect{x, y, tabWidth, tabHeight};
        DrawRoundedRect(rect, 18, active ? SDL_Color{77,223,202,255} : SDL_Color{255,255,255,235});
        TTF_Font* font = GetFont(34);
        int width = 0, height = 0;
        if (font != nullptr && TTF_SizeUTF8(font, label, &width, &height) == 0)
            DrawText(label, x + (rect.w-width)/2, rect.y + (rect.h-height)/2, 34,
                     active ? SDL_Color{0,0,0,255} : UiPalette::TopBarEnd);
    };
    const int optionsX = startX + buttonWidth + gap;
    const int previewX = optionsX + tabWidth + gap;
    const int applyX = previewX + tabWidth + gap;
    drawTab("OPTIONS", optionsX, frame.screen == Screen::Settings && frame.section == UiSection::Options);
    drawTab("PREVIEW", previewX, frame.screen == Screen::Preview || frame.screen == Screen::GeneratingPreview);
    drawTab("APPLY", applyX, frame.screen == Screen::Settings && frame.section == UiSection::Apply);
    const int zrX = applyX + tabWidth + gap;
    if (zrTexture_ != nullptr) {
        const SDL_Rect destination{zrX, (kTopBarHeight-buttonHeight)/2, buttonWidth, buttonHeight};
        SDL_RenderCopy(renderer_, zrTexture_, nullptr, &destination);
    } else DrawText("ZR", zrX + 24, y + 24, 25, UiPalette::White);
}

void TitlesSorterRenderer::DrawControllerHint(const char* button, const char* label, int x, int y) {
    const SDL_Rect capsule{x, y, 54, 42};
    DrawRoundedRect(capsule, 12, SDL_Color{255, 255, 255, 235});
    DrawText(button, x + 13, y + 6, 25, UiPalette::TopBarEnd);
    DrawText(label, x + 66, y + 5, 25, UiPalette::White);
}

void TitlesSorterRenderer::DrawBottomBar(const std::string& footer, UiFooterMode mode) {
    DrawGradient({0, LogicalHeight-kBottomBarHeight, LogicalWidth, kBottomBarHeight},
                 UiPalette::BottomBarStart, UiPalette::BottomBarEnd);
    const bool settings = footer.find("Navigate Options") != std::string::npos || footer.find("Navigate Actions") != std::string::npos || footer.find("Navigate Backups") != std::string::npos;
    const bool insideFolder = footer.find("Back to Home Menu") != std::string::npos;
    constexpr int fontSize = 36;
    constexpr int glyphSize = 46;
    TTF_Font* font = GetFont(fontSize);
    int textHeight = 0;
    if (font != nullptr) TTF_SizeUTF8(font, "Ag", nullptr, &textHeight);
    const int centerY = LogicalHeight - kBottomBarHeight / 2;
    const int textY = centerY - textHeight / 2;
    const SDL_Color color = UiPalette::White;
    constexpr int gap = 10;
    auto measureText = [&](const char* label) {
        int width = 0;
        if (font != nullptr) TTF_SizeUTF8(font, label, &width, nullptr);
        return width;
    };
    auto drawAction = [&](const char* glyph, const char* fallback, const char* label, int x) {
        int width = DrawGlyph(glyph, x, centerY, glyphSize, color);
        if (width == 0) { DrawText(fallback, x, textY, fontSize, color); width = 42; }
        DrawText(label, x + width + gap, textY, fontSize, color);
        return x + width + gap + measureText(label);
    };
    auto drawCenteredExit = [&]() {
        const int labelWidth = measureText("Exit");
        const int totalWidth = glyphSize + gap + labelWidth;
        const int x = (LogicalWidth-totalWidth)/2;
        return drawAction("\xee\x81\x84", "HOME", "Exit", x);
    };
    auto centeredInRightRegion = [&](const char* label, int left) {
        const int labelWidth = measureText(label);
        const int totalWidth = glyphSize + gap + labelWidth;
        constexpr int right = LogicalWidth - 24;
        return left + (right - left - totalWidth) / 2;
    };

    if (mode != UiFooterMode::Standard) {
        const int drawExitEnd = drawCenteredExit();
        if (mode == UiFooterMode::Decision) {
            // Decision frames retain the backup-list B/Cancel action. Place A/Confirm
            // in the remaining right-hand region without overlapping B/Cancel.
            constexpr int cancelX = LogicalWidth - 230;
            const int confirmWidth = glyphSize + gap + measureText("Confirm");
            const int confirmX = drawExitEnd + (cancelX - drawExitEnd - confirmWidth) / 2;
            drawAction("\xee\x80\x80", "A", "Confirm", confirmX);
            drawAction("\xee\x80\x81", "B", "Cancel", cancelX);
        } else {
            // Operation-result screens expose Exit and A/Confirm only.
            const int confirmX = centeredInRightRegion("Confirm", drawExitEnd);
            drawAction("\xee\x80\x80", "A", "Confirm", confirmX);
        }
        return;
    }

    if (settings) {
        const bool applyPanel = footer.find("Navigate Actions") != std::string::npos || footer.find("Navigate Backups") != std::string::npos;
        const char* navigation = footer.find("Navigate Backups") != std::string::npos ? "Navigate Backups" : (applyPanel ? "Navigate Actions" : "Navigate Options");
        int end = drawAction("\xee\x81\xbd", "D-PAD", navigation, 24); // E07D
        drawAction("\xee\x81\xbe", "D-PAD", "Change Option", end + 70); // E07E
        const int drawExitEnd = drawCenteredExit();
        const char* selectLabel = applyPanel ? "Select" : "Edit Text";
        const int editX = centeredInRightRegion(selectLabel, drawExitEnd);
        drawAction("\xee\x80\x80", "A", selectLabel, editX);
        if (footer.find("Navigate Backups") != std::string::npos)
            drawAction("\xee\x80\x81", "B", "Cancel", LogicalWidth - 230);
    } else {
        int end = drawAction("\xee\x81\x81", "D-PAD", "Navigate Titles", 24); // E041
        const int pairX = end + 70;
        int x = pairX;
        int width = DrawGlyph("\xee\x82\x83", x, centerY, glyphSize, color);
        if (width == 0) { DrawText("L", x, textY, fontSize, color); width = 42; }
        x += width + 4;
        width = DrawGlyph("\xee\x82\x84", x, centerY, glyphSize, color);
        if (width == 0) { DrawText("R", x, textY, fontSize, color); width = 42; }
        DrawText("Prev/Next Page", x + width + gap, textY, fontSize, color);
        const int drawExitEnd = drawCenteredExit();
        const char* action = insideFolder ? "Back to Home Menu" : "Enter Folder";
        const char* actionGlyph = insideFolder ? "\xee\x80\x81" : "\xee\x80\x80";
        const char* fallback = insideFolder ? "B" : "A";
        drawAction(actionGlyph, fallback, action, centeredInRightRegion(action, drawExitEnd));
    }
}

void TitlesSorterRenderer::DrawSettingsRow(const SettingsRowView& row, int y, int index) {
    const SDL_Rect card{72, y, 1776, index == 3 ? 220 : 142};
    if (row.selected) DrawRoundedBorder(card, 24, 5, UiPalette::Accent, UiPalette::SelectedFill);
    else DrawRoundedRect(card, 24, UiPalette::White);

    if (index == 3) {
        // Layout follows the supplied sketch: label and textbox share the top line;
        // explanatory copy sits beneath the label and ends before the textbox.
        const SDL_Rect textbox{1245, y + 22, 545, 116};
        const std::string label(row.label);
        int excludedLabelHeight = 0;
        TTF_Font* excludedLabelFont = GetFont(48);
        if (excludedLabelFont != nullptr)
            TTF_SizeUTF8(excludedLabelFont, label.c_str(), nullptr, &excludedLabelHeight);
        DrawText(label, 112,
                 textbox.y + (textbox.h - excludedLabelHeight) / 2, 48, UiPalette::Text,
                 label == "Excluded Pages" ? 560 : 1080);
        DrawRoundedBorder(textbox, 14, 2, row.selected ? UiPalette::Accent : UiPalette::Border,
                          SDL_Color{239,244,249,255});
        const std::string value = (label == "Excluded Pages" && (row.value.empty() || row.value == "None"))
            ? "Example: 1-3, 8, 11" : row.value;
        DrawText(value, textbox.x + 18, textbox.y + 37, 42, UiPalette::Muted, textbox.w-36);
        if (label == "Excluded Pages")
            DrawText("The excluded pages and their content won't be reordered.",
                     112, y + 151, 38, UiPalette::Muted, 1640);
        return;
    }

    constexpr int labelSize = 48;
    constexpr int valueSize = 52;
    TTF_Font* labelFont = GetFont(labelSize);
    int labelHeight = 0;
    if (labelFont != nullptr) TTF_SizeUTF8(labelFont, std::string(row.label).c_str(), nullptr, &labelHeight);
    DrawText(std::string(row.label), 112, y + (card.h-labelHeight)/2, labelSize, UiPalette::Text, 720);

    // Centre the value between chevrons. Reserve a symmetric group width based on
    // the longest selectable value, so changing to shorter values does not shift it.
    static const char* longestOptions[] = {"Product Code", "Mixed Alphabetically", "OFF"};
    int maxValueWidth = 0;
    TTF_Font* valueFont = GetFont(valueSize);
    if (valueFont != nullptr) {
        if (index == 0) {
            const char* options[] = {"Title ID", "Product Code", "Long Name", "Short Name"};
            for (const char* option : options) { int w=0; TTF_SizeUTF8(valueFont, option, &w, nullptr); maxValueWidth=std::max(maxValueWidth,w); }
        } else if (index == 1) {
            for (const char* option : longestOptions) { int w=0; TTF_SizeUTF8(valueFont, option, &w, nullptr); maxValueWidth=std::max(maxValueWidth,w); }
        } else TTF_SizeUTF8(valueFont, "OFF", &maxValueWidth, nullptr);
    }
    int currentWidth = 0;
    if (valueFont != nullptr) TTF_SizeUTF8(valueFont, row.value.c_str(), &currentWidth, nullptr);
    const int centreX = 1450;
    const int valueX = centreX-currentWidth/2;
    const int chevronOffset = maxValueWidth/2 + 54;
    const int textHeight = [&]() { int h=0; if(valueFont) TTF_SizeUTF8(valueFont,row.value.c_str(),nullptr,&h); return h; }();
    const int valueY = y + (card.h-textHeight)/2;
    DrawText("<", centreX-chevronOffset-30, y+(card.h-52)/2, 52, UiPalette::TopBarEnd);
    DrawText(row.value, valueX, valueY, valueSize, row.selected ? UiPalette::TopBarEnd : UiPalette::Muted,
             maxValueWidth+4);
    DrawText(">", centreX+chevronOffset-8, y+(card.h-52)/2, 52, UiPalette::TopBarEnd);
}

void TitlesSorterRenderer::DrawApply(const UiFrame& frame) {
    // Decision and operation-result frames use an uncluttered, full-screen layout.
    if (frame.applyConfirmation != ApplyConfirmation::None) {
        const auto drawCentred = [this](const std::string& text, int y,
                                        int size, SDL_Color color) {
            if (text.empty()) return;
            int width = 0;
            TTF_Font* font = GetFont(size);
            if (font != nullptr) TTF_SizeUTF8(font, text.c_str(), &width, nullptr);
            DrawText(text, (LogicalWidth - width) / 2, y, size, color, LogicalWidth - 96);
        };
        const auto drawWrappedCentred = [this](const std::string& text, int topY,
                                                int size, int maximumWidth,
                                                SDL_Color color) {
            if (text.empty()) return;
            TTF_Font* font = GetFont(size);
            if (font == nullptr) return;
            std::vector<std::string> lines;
            std::string line;
            std::string token;
            const auto appendToken = [&]() {
                if (token.empty()) return;
                std::string candidate = line.empty() ? token : line + " " + token;
                int width = 0;
                TTF_SizeUTF8(font, candidate.c_str(), &width, nullptr);
                if (width <= maximumWidth) {
                    line = std::move(candidate);
                } else {
                    if (!line.empty()) lines.push_back(std::move(line));
                    line.clear();
                    std::string part;
                    for (char ch : token) {
                        part.push_back(ch);
                        int partWidth = 0;
                        TTF_SizeUTF8(font, part.c_str(), &partWidth, nullptr);
                        if (partWidth > maximumWidth && part.size() > 1U) {
                            const char last = part.back();
                            part.pop_back();
                            lines.push_back(std::move(part));
                            part.assign(1U, last);
                        }
                    }
                    line = std::move(part);
                }
                token.clear();
            };
            for (char ch : text) {
                if (ch == ' ' || ch == '\n' || ch == '\t') {
                    appendToken();
                    if (ch == '\n' && !line.empty()) {
                        lines.push_back(std::move(line));
                        line.clear();
                    }
                } else token.push_back(ch);
            }
            appendToken();
            if (!line.empty()) lines.push_back(std::move(line));
            int y = topY;
            for (const std::string& wrappedLine : lines) {
                int width = 0, height = 0;
                TTF_SizeUTF8(font, wrappedLine.c_str(), &width, &height);
                DrawText(wrappedLine, (LogicalWidth - width) / 2, y, size,
                         color, maximumWidth);
                y += height + 8;
            }
        };

        switch (frame.applyConfirmation) {
            case ApplyConfirmation::RestoreDecision:
                drawCentred("Are you sure you want to restore this backup?", 420, 53, UiPalette::Text);
                drawWrappedCentred(frame.dialogDetail, 555, 38, LogicalWidth - 160, UiPalette::Muted);
                break;
            case ApplyConfirmation::ApplyLayoutDecision:
                drawWrappedCentred("Are you sure you want to\napply the previewed layout?",
                                    455, 53, LogicalWidth - 160, UiPalette::Text);
                break;
            case ApplyConfirmation::BackupResult:
                drawCentred(frame.message, 420, 60,
                            frame.message == "Backup Completed !!!" ? UiPalette::Text : UiPalette::Error);
                drawWrappedCentred(frame.dialogDetail, 555, 35, LogicalWidth - 160, UiPalette::Muted);
                break;
            case ApplyConfirmation::RestoreResult:
                drawCentred(frame.message, 420, 60,
                            frame.message == "Backup Restored !!!" ? UiPalette::Text : UiPalette::Error);
                drawWrappedCentred(frame.dialogDetail, 555, 35, LogicalWidth - 160, UiPalette::Muted);
                break;
            case ApplyConfirmation::ApplyResult:
                drawCentred(frame.message, 420, 60,
                            frame.message == "Layout Applied !!!" ? UiPalette::Text : UiPalette::Error);
                drawWrappedCentred(frame.dialogDetail, 555, 35, LogicalWidth - 160, UiPalette::Muted);
                break;
            case ApplyConfirmation::PreviewRequired:
                drawCentred(frame.message, 420, 60, UiPalette::Text);
                break;
            case ApplyConfirmation::None:
                break;
        }
        return;
    }

    DrawText(frame.showingBackupList ? "Restore Backup" : "Apply and Backups", 72, 158, 57, UiPalette::Text);
    if (frame.showingBackupList) {
        DrawText("Select a backup.", 72, 230, 38, UiPalette::Muted);
        DrawText("Manual and automatic backups are listed newest first.", 72, 276, 38, UiPalette::Muted);
        constexpr int firstY = 340;
        constexpr int rowHeight = 92;
        if (frame.backupNames.empty()) {
            DrawText("No backups found.", 112, 400, 42, UiPalette::Muted);
        } else {
            const std::size_t first = frame.selectedBackup > 3U ? frame.selectedBackup - 3U : 0U;
            const std::size_t count = std::min<std::size_t>(7U, frame.backupNames.size() - first);
            for (std::size_t i = 0; i < count; ++i) {
                const std::size_t index = first + i;
                const SDL_Rect card{72, firstY + static_cast<int>(i) * rowHeight, 1776, 76};
                if (index == frame.selectedBackup) DrawRoundedBorder(card, 16, 4, UiPalette::Accent, UiPalette::SelectedFill);
                else DrawRoundedRect(card, 16, UiPalette::White);
                const std::string& fullPath = frame.backupNames[index];
                const std::size_t separator = fullPath.find_last_of("/\\");
                const std::string displayName = separator == std::string::npos ? fullPath : fullPath.substr(separator + 1U);
                DrawText(displayName, 108, card.y + 17, 32, index == frame.selectedBackup ? UiPalette::TopBarEnd : UiPalette::Text, 1680);
            }
        }
        return;
    }

    DrawText("Manage save backups and apply the layout currently shown in Preview.",
             72, 230, 38, UiPalette::Muted);

    constexpr int controlX = 72;
    constexpr int controlWidth = 1776;
    constexpr int controlHeight = 104;
    constexpr int firstY = 270;
    constexpr int regularGap = 10;
    constexpr int majorGap = 70;
    constexpr int applyBlockOffset = 25;
    const int autoY = firstY + applyBlockOffset;
    const int maxY = autoY + controlHeight + regularGap;
    const int createY = maxY + controlHeight + majorGap;
    const int restoreY = createY + controlHeight + regularGap;
    const int applyY = restoreY + controlHeight + majorGap - applyBlockOffset;

    const auto drawCard = [this](const SDL_Rect& rect, bool selected) {
        if (selected) DrawRoundedBorder(rect, 20, 4, UiPalette::Accent, UiPalette::SelectedFill);
        else DrawRoundedRect(rect, 20, UiPalette::White);
    };
    const SDL_Rect autoRect{controlX, autoY, controlWidth, controlHeight};
    drawCard(autoRect, frame.applyRows[0].selected);
    TTF_Font* labelFont = GetFont(48);
    int labelHeight = 0;
    if (labelFont != nullptr) TTF_SizeUTF8(labelFont, "Autobackup", nullptr, &labelHeight);
    DrawText("Autobackup", controlX + 40, autoY + (controlHeight - labelHeight) / 2,
             48, UiPalette::Text);
    TTF_Font* valueFont = GetFont(52);
    // Keep the value group visually balanced and shift it left by one character
    // relative to the previous alignment. The explicit spaces produce
    // the requested "<  Enabled  >" / "<  Disabled  >" presentation.
    const std::string autoBackupValue = "  " + std::string(frame.applyRows[0].value) + "  ";
    int autoValueWidth = 0, autoValueHeight = 0;
    int bracketWidth = 0, bracketHeight = 0, characterWidth = 0;
    if (valueFont != nullptr) {
        TTF_SizeUTF8(valueFont, autoBackupValue.c_str(), &autoValueWidth, &autoValueHeight);
        TTF_SizeUTF8(valueFont, "<", &bracketWidth, &bracketHeight);
        TTF_SizeUTF8(valueFont, "M", &characterWidth, nullptr);
    }
    const int groupGap = 4;
    const int groupWidth = bracketWidth + groupGap + autoValueWidth + groupGap + bracketWidth;
    const int groupCenterX = controlX + controlWidth - 230 - characterWidth;
    const int groupX = groupCenterX - groupWidth / 2;
    const int bracketY = autoY + (controlHeight - bracketHeight) / 2;
    DrawText("<", groupX, bracketY, 52, UiPalette::TopBarEnd);
    DrawText(autoBackupValue, groupX + bracketWidth + groupGap,
             autoY + (controlHeight - autoValueHeight) / 2, 52,
             frame.applyRows[0].selected ? UiPalette::TopBarEnd : UiPalette::Muted);
    DrawText(">", groupX + bracketWidth + groupGap + autoValueWidth + groupGap,
             bracketY, 52, UiPalette::TopBarEnd);

    const SDL_Rect maxRect{controlX, maxY, controlWidth, controlHeight};
    drawCard(maxRect, frame.applyRows[1].selected);
    DrawText(std::string(frame.applyRows[1].label), controlX + 40,
             maxY + (controlHeight - labelHeight) / 2, 48, UiPalette::Text, 1050);
    const SDL_Rect numberField{controlX + controlWidth - 545, maxY + 10, 505, controlHeight - 20};
    DrawRoundedBorder(numberField, 14, 2,
                      frame.applyRows[1].selected ? UiPalette::Accent : UiPalette::Border,
                      SDL_Color{239,244,249,255});
    int numberWidth = 0, numberHeight = 0;
    if (valueFont != nullptr) TTF_SizeUTF8(valueFont, frame.applyRows[1].value.c_str(),
                                           &numberWidth, &numberHeight);
    DrawText(frame.applyRows[1].value,
             numberField.x + (numberField.w - numberWidth) / 2,
             numberField.y + (numberField.h - numberHeight) / 2,
             52, frame.applyRows[1].selected ? UiPalette::TopBarEnd : UiPalette::Muted);

    const auto drawActionCard = [&](int y, const ApplyRowView& row) {
        const SDL_Rect rect{controlX, y, controlWidth, controlHeight};
        drawCard(rect, row.selected);
        int textWidth = 0, textHeight = 0;
        if (labelFont != nullptr) TTF_SizeUTF8(labelFont, std::string(row.label).c_str(),
                                                &textWidth, &textHeight);
        DrawText(std::string(row.label), rect.x + (rect.w - textWidth) / 2,
                 rect.y + (rect.h - textHeight) / 2, 48,
                 row.interactive ? UiPalette::Text : UiPalette::Muted, rect.w - 48);
    };
    drawActionCard(createY, frame.applyRows[2]);
    drawActionCard(restoreY, frame.applyRows[3]);
    drawActionCard(applyY, frame.applyRows[4]);

    if (!frame.message.empty()) {
        const bool failed = frame.message.find("failed") != std::string::npos ||
                            frame.message.find("Unable") != std::string::npos ||
                            frame.message.find("invalid") != std::string::npos;
        DrawText(frame.message, 72, 958, 24, failed ? UiPalette::Error : UiPalette::Muted, 1776);
    }
}

void TitlesSorterRenderer::DrawSettings(const UiFrame& frame) {
    DrawText("Sorting Options", 72, 158, 57, UiPalette::Text);
    DrawText("Choose how your Home Menu titles and folders should be arranged.",
             72, 230, 38, UiPalette::Muted);
    constexpr int firstY = 285;
    constexpr int rowGap = 151;
    for (std::size_t i = 0; i < frame.settings.size(); ++i)
        DrawSettingsRow(frame.settings[i], firstY + static_cast<int>(i) * rowGap, static_cast<int>(i));
    if (!frame.message.empty()) DrawText(frame.message, 72, 958, 24, UiPalette::Error, 1776);
}

void TitlesSorterRenderer::DrawLoading(std::uint32_t elapsedMilliseconds) {
    const SDL_Rect panel{590, 280, 740, 540};
    DrawRoundedBorder(panel, 30, 4, UiPalette::Accent, UiPalette::White);
    auto drawPanelCentredText = [&](const char* value, int y, int size, SDL_Color color) {
        int width = 0;
        TTF_Font* font = GetFont(size);
        if (font != nullptr) TTF_SizeUTF8(font, value, &width, nullptr);
        DrawText(value, panel.x + (panel.w - width) / 2, y, size, color, panel.w - 48);
    };
    drawPanelCentredText("Loading Home Menu", 345, 63, UiPalette::Text);
    drawPanelCentredText("Reading the Home Menu save and", 435, 36, UiPalette::Muted);
    constexpr int descriptionY = 480;
    constexpr int descriptionSize = 36;
    const char* description = "installed title metadata...";
    drawPanelCentredText(description, descriptionY, descriptionSize, UiPalette::Muted);

    // Centre the spinner in the free vertical space between the final text
    // line and the inner bottom edge of the loading panel.
    int descriptionHeight = 0;
    TTF_Font* descriptionFont = GetFont(descriptionSize);
    if (descriptionFont != nullptr)
        TTF_SizeUTF8(descriptionFont, description, nullptr, &descriptionHeight);
    constexpr int panelBottom = 280 + 540;
    constexpr int bottomInset = 36;
    const int textBottom = descriptionY + descriptionHeight;
    const int spinnerCenterY = textBottom +
        ((panelBottom - bottomInset) - textBottom) / 2;

    constexpr int dotCount = 12;
    constexpr int centerX = LogicalWidth / 2;
    const int centerY = spinnerCenterY;
    constexpr int ringRadius = 82;
    const unsigned active = (elapsedMilliseconds / 100U) % dotCount;
    for (unsigned i = 0; i < dotCount; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(dotCount)) * 2.0f * kPi - kPi / 2.0f;
        const int x = centerX + static_cast<int>(std::cos(angle) * ringRadius);
        const int y = centerY + static_cast<int>(std::sin(angle) * ringRadius);
        const int distance = static_cast<int>((i + dotCount - active) % dotCount);
        const Uint8 alpha = static_cast<Uint8>(255 - std::min(distance, 8) * 22);
        DrawRoundedRect({x - 9, y - 9, 18, 18}, 9, SDL_Color{62, 174, 255, alpha});
    }
}

void TitlesSorterRenderer::DrawGenerating(std::uint32_t elapsedMilliseconds) {
    const SDL_Rect panel{590, 330, 740, 420};
    DrawRoundedBorder(panel, 30, 4, UiPalette::Accent, UiPalette::White);
    DrawText("Generating Preview", 770, 385, 42, UiPalette::Text);
    DrawText("Analysing the current Home Menu layout...", 690, 445, 26, UiPalette::Muted);

    constexpr int dotCount = 12;
    constexpr int centerX = LogicalWidth / 2;
    constexpr int centerY = 610;
    constexpr int ringRadius = 82;
    const unsigned active = (elapsedMilliseconds / 100U) % dotCount;
    for (unsigned i = 0; i < dotCount; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(dotCount)) * 2.0f * kPi - kPi / 2.0f;
        const int x = centerX + static_cast<int>(std::cos(angle) * ringRadius);
        const int y = centerY + static_cast<int>(std::sin(angle) * ringRadius);
        const int distance = static_cast<int>((i + dotCount - active) % dotCount);
        const Uint8 alpha = static_cast<Uint8>(255 - std::min(distance, 8) * 22);
        DrawRoundedRect({x - 9, y - 9, 18, 18}, 9,
                        SDL_Color{62, 174, 255, alpha});
    }
}

SDL_Texture* TitlesSorterRenderer::GetTitleIcon(const PreviewItem& item) {
    if (renderer_ == nullptr) return nullptr;

    const bool hasMemoryIcon = item.iconData != nullptr && !item.iconData->empty();
    std::string cacheKey;
    if (hasMemoryIcon) {
        cacheKey = "memory:" + std::to_string(item.titleId);
    } else {
        if (item.iconPath.empty()) return nullptr;
        cacheKey = item.iconPath;
    }

    const auto cached = iconTextures_.find(cacheKey);
    if (cached != iconTextures_.end()) return cached->second;

    SDL_Surface* surface = nullptr;
    if (hasMemoryIcon) {
        const auto& bytes = *item.iconData;
        if (bytes.size() <= static_cast<std::size_t>(0x7FFFFFFF)) {
            SDL_RWops* stream = SDL_RWFromConstMem(bytes.data(), static_cast<int>(bytes.size()));
            if (stream != nullptr) surface = IMG_Load_RW(stream, 1);
            if (surface == nullptr) surface = DecodeTgaSurface(bytes);
        }
    } else {
        // Installed title icons are loaded from the MCP-derived path. Retry
        // alternate CafeOS/devoptab forms and special Disc/Wii locations.
        surface = LoadTitleIconSurface(item);
    }

    if (surface == nullptr) {
#if defined(__WIIU__)
        TitleSorterLogf("Renderer", "Unable to decode title icon for ID %016llX (memory=%u, bytes=%u): %s",
                        static_cast<unsigned long long>(item.titleId),
                        hasMemoryIcon ? 1U : 0U,
                        hasMemoryIcon ? static_cast<unsigned int>(item.iconData->size()) : 0U,
                        IMG_GetError());
#endif
        iconTextures_.emplace(cacheKey, nullptr);
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
#if defined(__WIIU__)
    if (texture == nullptr)
        TitleSorterLogf("Renderer", "SDL_CreateTextureFromSurface failed for title ID %016llX: %s",
                        static_cast<unsigned long long>(item.titleId), SDL_GetError());
#endif
    constexpr std::size_t iconCacheLimit = 48U;
    if (iconTextures_.size() >= iconCacheLimit) {
        auto victim = iconTextures_.begin();
        if (victim->second != nullptr) SDL_DestroyTexture(victim->second);
        iconTextures_.erase(victim);
    }
    iconTextures_.emplace(std::move(cacheKey), texture);
    return texture;
}

void TitlesSorterRenderer::DrawPreviewItem(const PreviewItem& item, const SDL_Rect& rect) {
    DrawRoundedRect(rect, 18, UiPalette::White);
    if (!item.occupied) {
        DrawRoundedRect({rect.x + 18, rect.y + 18, rect.w - 36, rect.h - 66},
                        14, UiPalette::EmptySlot);
        DrawText("EMPTY", rect.x + 28, rect.y + rect.h - 39, 16, UiPalette::Muted,
                 rect.w - 56);
        return;
    }

    SDL_Color tileColor = UiPalette::Title;
    const char* typeLabel = "TITLE";
    switch (item.kind) {
        case ItemKind::Folder: tileColor = UiPalette::Folder; typeLabel = "FOLDER"; break;
        case ItemKind::Disc: tileColor = SDL_Color{112, 129, 148, 255}; typeLabel = "DISC"; break;
        case ItemKind::Vwii: tileColor = SDL_Color{145, 115, 190, 255}; typeLabel = "WII"; break;
        case ItemKind::Unknown: tileColor = UiPalette::Pinned; typeLabel = "UNKNOWN"; break;
        case ItemKind::Empty: break;
        case ItemKind::Title: break;
    }
    // Artwork is rendered square and centred. Missing/unreadable title artwork
    // uses the bundled generic icon instead of a misleading letter glyph.
    const int iconSize = std::min(rect.w - 36, rect.h - 72);
    const SDL_Rect iconRect{rect.x + (rect.w - iconSize) / 2,
                            rect.y + 16, iconSize, iconSize};

    if (item.kind == ItemKind::Title || item.kind == ItemKind::Disc ||
        item.kind == ItemKind::Vwii || item.kind == ItemKind::Unknown) {
        SDL_Texture* icon = GetTitleIcon(item);
        if (icon == nullptr) icon = unknownTexture_;
        if (icon != nullptr) {
            int textureWidth = 0;
            int textureHeight = 0;
            if (SDL_QueryTexture(icon, nullptr, nullptr, &textureWidth, &textureHeight) == 0 &&
                textureWidth > 0 && textureHeight > 0) {
                const float sourceWidth = static_cast<float>(textureWidth);
                const float sourceHeight = static_cast<float>(textureHeight);
                const float scale = std::min(static_cast<float>(iconRect.w) / sourceWidth,
                                             static_cast<float>(iconRect.h) / sourceHeight);
                const int drawWidth = std::max(1, static_cast<int>(sourceWidth * scale));
                const int drawHeight = std::max(1, static_cast<int>(sourceHeight * scale));
                const SDL_Rect fittedIcon{iconRect.x + (iconRect.w - drawWidth) / 2,
                                          iconRect.y + (iconRect.h - drawHeight) / 2,
                                          drawWidth, drawHeight};
                SDL_RenderCopy(renderer_, icon, nullptr, &fittedIcon);
            } else {
                SDL_RenderCopy(renderer_, icon, nullptr, &iconRect);
            }
        } else {
            DrawRoundedRect(iconRect, 15, tileColor);
        }
        DrawText(item.label.empty() ? typeLabel : item.label,
                 rect.x + 14, rect.y + rect.h - 48, 27, UiPalette::Text, rect.w - 28);
    } else if (item.kind == ItemKind::Folder) {
        const SDL_Rect folderArea{rect.x + 8, rect.y + 6, rect.w - 16, rect.h - 12};
        std::size_t colorIndex = static_cast<std::size_t>(item.folderColor);
        if (colorIndex >= folderTextures_.size()) colorIndex = 0U;
        SDL_Texture* folderTexture = folderTextures_[colorIndex];
        if (folderTexture != nullptr) {
            int sourceWidth = 0, sourceHeight = 0;
            SDL_QueryTexture(folderTexture, nullptr, nullptr, &sourceWidth, &sourceHeight);
            if (sourceWidth > 0 && sourceHeight > 0) {
                const float fit = std::min(static_cast<float>(folderArea.w) / static_cast<float>(sourceWidth),
                                           static_cast<float>(folderArea.h) / static_cast<float>(sourceHeight));
                const int drawWidth = static_cast<int>(static_cast<float>(sourceWidth) * fit);
                const int drawHeight = static_cast<int>(static_cast<float>(sourceHeight) * fit);
                const SDL_Rect folderRect{folderArea.x + (folderArea.w - drawWidth) / 2,
                                          folderArea.y + (folderArea.h - drawHeight) / 2,
                                          drawWidth, drawHeight};
                SDL_RenderCopy(renderer_, folderTexture, nullptr, &folderRect);
            }
        } else {
            static constexpr SDL_Color folderFallbackColors[] = {
                {65, 177, 218, 255}, {77, 223, 54, 255},
                {240, 202, 5, 255}, {249, 145, 48, 255},
                {236, 79, 88, 255}, {252, 116, 182, 255},
                {177, 94, 244, 255}, {128, 128, 128, 255}
            };
            DrawRoundedRect(folderArea, 15, folderFallbackColors[colorIndex]);
        }
        const std::string folderName = item.label.empty() ? typeLabel : item.label;
        TTF_Font* font = GetFont(34);
        int textWidth = 0, textHeight = 0;
        if (font != nullptr) {
            TTF_SetFontStyle(font, TTF_STYLE_BOLD);
            if (TTF_SizeUTF8(font, folderName.c_str(), &textWidth, &textHeight) == 0) {
                const SDL_Color folderTextColor = colorIndex == static_cast<std::size_t>(FolderColor::Grey)
                    ? SDL_Color{65, 180, 220, 255} : SDL_Color{0, 0, 0, 255};
                DrawText(folderName, folderArea.x + (folderArea.w - std::min(textWidth, folderArea.w - 24)) / 2,
                         folderArea.y + (folderArea.h - textHeight) / 2, 34, folderTextColor, folderArea.w - 24);
            }
            TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
        }
    }

    if (item.pinned && lockTexture_ != nullptr) {
        // The lock marks the entire tile as fixed. Anchor it to the card's
        // upper-right corner so it stays clear of the title artwork and label.
        constexpr int lockSize = 60; // 2x the previous 30 px overlay.
        constexpr int lockInset = 8;
        const SDL_Rect lockRect{rect.x + rect.w - lockSize - lockInset,
                                rect.y + lockInset, lockSize, lockSize};
        SDL_RenderCopy(renderer_, lockTexture_, nullptr, &lockRect);
    }
}

void TitlesSorterRenderer::DrawPreview(const UiFrame& frame) {
    DrawText(frame.insideFolder ? frame.previewLocation : "Home Menu", 72, 164, 56, UiPalette::Text);
    char pageText[64]{};
    std::snprintf(pageText, sizeof(pageText), "Page %u / %u", frame.preview.pageNumber,
                  std::max(1U, frame.previewPageCount));
    DrawText(pageText, 1610, 174, 38, UiPalette::Muted);

    // The 5x3 grid is an experimental visualization of the currently inferred
    // page mapping; it is deliberately labelled as a preview, not a verified map.
    constexpr int columns = 5;
    constexpr int rows = 3;
    constexpr int tileWidth = 300;
    constexpr int tileHeight = 210;
    constexpr int gapX = 42;
    constexpr int gapY = 20;
    constexpr int totalWidth = columns * tileWidth + (columns - 1) * gapX;
    constexpr int startX = (LogicalWidth - totalWidth) / 2;
    int headingHeight = 0;
    TTF_Font* headingFont = GetFont(56);
    if (headingFont != nullptr)
        TTF_SizeUTF8(headingFont, (frame.insideFolder ? frame.previewLocation : std::string("Home Menu")).c_str(),
                     nullptr, &headingHeight);
    const int headingBottom = 164 + headingHeight;
    constexpr int gridHeight = rows * tileHeight + (rows - 1) * gapY;
    const int availableHeight = (LogicalHeight - kBottomBarHeight) - headingBottom;
    const int startY = headingBottom + (availableHeight - gridHeight) / 2;
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const int index = row * columns + column;
            const SDL_Rect rect{startX + column * (tileWidth + gapX),
                                startY + row * (tileHeight + gapY), tileWidth, tileHeight};
            DrawPreviewItem(frame.preview.slots[static_cast<std::size_t>(index)], rect);
        }
    }
    if (!frame.message.empty()) DrawText(frame.message, 72, 950, 18, UiPalette::Error, 1776);
}

void TitlesSorterRenderer::Draw(const UiFrame& frame, std::uint32_t elapsedMilliseconds) {
    if (!initialized_ || renderer_ == nullptr) return;
    DrawBackground();
    DrawTopBar(frame);
    switch (frame.screen) {
        case Screen::Settings: if (frame.section == UiSection::Apply) DrawApply(frame); else DrawSettings(frame); break;
        case Screen::LoadingData: DrawLoading(elapsedMilliseconds); break;
        case Screen::GeneratingPreview: DrawGenerating(elapsedMilliseconds); break;
        case Screen::Preview: DrawPreview(frame); break;
        case Screen::Error:
            DrawText("Unable to load Home Menu data", 72, 220, 42, UiPalette::Error);
            DrawText(frame.message, 72, 300, 28, UiPalette::Muted, 1700);
            break;
    }
    if (frame.screen == Screen::Preview && frame.previewSelected && frame.selectedPreviewSlot < 15U) {
        constexpr int columns = 5, rows = 3;
        constexpr int tileWidth = 300, tileHeight = 210, gapX = 42, gapY = 20;
        const int totalWidth = columns * tileWidth + (columns - 1) * gapX;
        const int x = (LogicalWidth-totalWidth)/2 +
                      static_cast<int>(frame.selectedPreviewSlot%5U)*(tileWidth+gapX);

        // Match the dynamically centred grid origin calculated in DrawPreview().
        int headingHeight = 0;
        TTF_Font* headingFont = GetFont(56);
        const std::string heading = frame.insideFolder ? frame.previewLocation : "Home Menu";
        if (headingFont != nullptr)
            TTF_SizeUTF8(headingFont, heading.c_str(), nullptr, &headingHeight);
        const int headingBottom = 164 + headingHeight;
        constexpr int gridHeight = rows * tileHeight + (rows - 1) * gapY;
        const int availableHeight = (LogicalHeight - kBottomBarHeight) - headingBottom;
        const int gridStartY = headingBottom + (availableHeight - gridHeight) / 2;
        const int y = gridStartY +
                      static_cast<int>(frame.selectedPreviewSlot/5U)*(tileHeight+gapY);

        SDL_SetRenderDrawColor(renderer_, UiPalette::Accent.r, UiPalette::Accent.g,
                               UiPalette::Accent.b, 255);
        for (int inset = 0; inset < 8; ++inset) {
            SDL_Rect outline{x + inset, y + inset, tileWidth - inset * 2, tileHeight - inset * 2};
            SDL_RenderDrawRect(renderer_, &outline);
        }
    }
    DrawBottomBar(frame.screen == Screen::LoadingData ? "Navigate Options" : frame.footer,
                  frame.footerMode);
    SDL_RenderPresent(renderer_);
}

} // namespace titles_sorter
