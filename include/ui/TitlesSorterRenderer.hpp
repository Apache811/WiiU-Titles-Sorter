#pragma once

#include "ui/TitlesSorterUI.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstddef>
#include <cstdint>
#include <array>
#include <string>
#include <unordered_map>
#include <deque>
#include <vector>

namespace titles_sorter {

struct UiPalette {
    static constexpr SDL_Color Background{239, 244, 249, 255};
    static constexpr SDL_Color TopBarStart{27, 139, 224, 255};
    static constexpr SDL_Color TopBarEnd{0, 91, 174, 255};
    static constexpr SDL_Color BottomBarStart{0, 91, 174, 255};
    static constexpr SDL_Color BottomBarEnd{27, 139, 224, 255};
    static constexpr SDL_Color Text{13, 27, 57, 255};
    static constexpr SDL_Color Muted{81, 105, 135, 255};
    static constexpr SDL_Color White{255, 255, 255, 255};
    static constexpr SDL_Color Accent{62, 174, 255, 255};
    static constexpr SDL_Color SelectedFill{211, 234, 253, 255};
    static constexpr SDL_Color Border{166, 192, 215, 255};
    static constexpr SDL_Color EmptySlot{224, 233, 241, 255};
    static constexpr SDL_Color Folder{255, 190, 74, 255};
    static constexpr SDL_Color Title{80, 159, 224, 255};
    static constexpr SDL_Color Pinned{142, 155, 171, 255};
    static constexpr SDL_Color Error{198, 40, 40, 255};
};

class TitlesSorterRenderer {
public:
    static constexpr int LogicalWidth = 1920;
    static constexpr int LogicalHeight = 1080;

    TitlesSorterRenderer() = default;
    ~TitlesSorterRenderer();

    TitlesSorterRenderer(const TitlesSorterRenderer&) = delete;
    TitlesSorterRenderer& operator=(const TitlesSorterRenderer&) = delete;

    // The font bytes are owned by the caller and must remain valid only for this call.
    // SDL_ttf copies the font data into its own RW stream during initialization.
    bool Initialize(SDL_Renderer* renderer, const void* fontData, std::size_t fontSize,
                    std::string& error);
    void Shutdown() noexcept;
    void Draw(const UiFrame& frame, std::uint32_t elapsedMilliseconds);

private:
    struct CachedTexture {
        SDL_Texture* texture{};
        int width{};
        int height{};
    };

    struct FontEntry {
        int size{};
        TTF_Font* font{};
    };

    SDL_Renderer* renderer_{};
    std::vector<FontEntry> fonts_;
    std::vector<std::uint8_t> fontData_;
    void* iconFontData_{};
    std::uint32_t iconFontSize_{};
    std::unordered_map<int, TTF_Font*> iconFonts_;
    std::unordered_map<std::string, CachedTexture> glyphCache_;
    std::unordered_map<std::string, CachedTexture> textCache_;
    std::deque<std::string> cacheOrder_;
    SDL_Texture* zlTexture_{};
    SDL_Texture* zrTexture_{};
    SDL_Texture* unknownTexture_{};
    SDL_Texture* lockTexture_{};
    // Indexed by FolderColor's on-disk value (0..7). Folder textures are
    // pre-scaled to 256x235 by the build-time asset pipeline.
    std::array<SDL_Texture*, 8> folderTextures_{};
    std::unordered_map<std::string, SDL_Texture*> iconTextures_;
    bool initialized_{};

    [[nodiscard]] TTF_Font* GetFont(int size);
    [[nodiscard]] CachedTexture* GetTextTexture(const std::string& text, int size, SDL_Color color);
    void DrawText(const std::string& text, int x, int y, int size, SDL_Color color,
                  int maxWidth = 0);
    int DrawGlyph(const char* glyph, int x, int centerY, int size, SDL_Color color);
    void DrawBackground();
    void DrawTopBar(const UiFrame& frame);
    void DrawBottomBar(const std::string& footer, UiFooterMode mode = UiFooterMode::Standard);
    void DrawSettings(const UiFrame& frame);
    void DrawApply(const UiFrame& frame);
    void DrawLoading(std::uint32_t elapsedMilliseconds);
    void DrawGenerating(std::uint32_t elapsedMilliseconds);
    void DrawPreview(const UiFrame& frame);
    void DrawRoundedRect(const SDL_Rect& rect, int radius, SDL_Color color);
    void DrawRoundedBorder(const SDL_Rect& rect, int radius, int thickness, SDL_Color color,
                           SDL_Color innerColor);
    void DrawGradient(const SDL_Rect& rect, SDL_Color top, SDL_Color bottom);
    void DrawSettingsRow(const SettingsRowView& row, int y, int index);
    void DrawPreviewItem(const PreviewItem& item, const SDL_Rect& rect);
    void DrawControllerHint(const char* button, const char* label, int x, int y);
    SDL_Texture* GetTitleIcon(const PreviewItem& item);
};

} // namespace titles_sorter
