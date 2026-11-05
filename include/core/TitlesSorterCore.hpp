#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace titles_sorter {

inline constexpr std::size_t kExpectedSaveSize = 0x1FC44;
inline constexpr std::size_t kRecordSize = 16;
inline constexpr std::size_t kMainSlotCount = 300;
inline constexpr std::size_t kPageCapacity = 15; // Experimentally supported; not an official format guarantee.
inline constexpr std::size_t kFolderCount = 60;
inline constexpr std::size_t kFolderSlotCount = 60;
inline constexpr std::size_t kFolderStride = 0x7B8;
inline constexpr std::size_t kMainNandOffset = 0x0000;
inline constexpr std::size_t kMainUsbOffset = 0x12C0;
inline constexpr std::size_t kOpaqueRegionOffset = 0x2580;
inline constexpr std::size_t kFolderRegionOffset = 0x2D24;
inline constexpr std::size_t kMainSelectionOffset = 0x2587;
inline constexpr std::size_t kFolderMetadataOffsetInBlock = 0x780;
inline constexpr std::size_t kFolderMetadataSize = 56;
inline constexpr std::size_t kFolderRememberedSlotOffset = 0x2F;
// Experimentally confirmed in BaristaAccountSaveFile_15/16/17.dat:
// metadata byte 0x33 stores the Wii U Home Menu folder colour.
inline constexpr std::size_t kFolderColorOffsetInMetadata = 0x33;
static_assert(kFolderColorOffsetInMetadata < kFolderMetadataSize);

enum class FolderColor : std::uint8_t {
    Blue = 0x00,
    Green = 0x01,
    Yellow = 0x02,
    Orange = 0x03,
    Red = 0x04,
    Rose = 0x05,
    Purple = 0x06,
    Grey = 0x07
};

enum class ItemKind : std::uint8_t {
    Empty,
    Title,
    Folder,
    Disc,
    Vwii,
    Unknown
};

struct RawRecord {
    std::array<std::uint8_t, kRecordSize> bytes{};
    std::uint64_t id{};
    std::uint32_t type{};
    std::uint32_t extra{};
};

struct Slot {
    RawRecord nand;
    RawRecord usb;
    ItemKind kind{ItemKind::Empty};
    std::uint64_t titleId{};
    std::uint32_t folderId{};
    bool occupied{};
};

struct Folder {
    std::uint32_t id{};
    std::string name;
    std::array<std::uint8_t, kFolderMetadataSize> metadata{};
    FolderColor color{FolderColor::Blue};
    std::uint8_t rememberedChildSlotCandidate{};
    std::array<Slot, kFolderSlotCount> slots{};
    bool active{};
};

struct SaveModel {
    std::vector<std::uint8_t> originalBytes;
    std::array<Slot, kMainSlotCount> mainSlots{};
    std::array<Folder, kFolderCount> folders{};
    std::uint8_t mainSelectionCandidate{};
    std::vector<std::string> warnings;
};

struct ParseResult {
    std::unique_ptr<SaveModel> model;
    std::vector<std::string> errors;
    explicit operator bool() const noexcept { return static_cast<bool>(model) && errors.empty(); }
};

enum class SortField : std::uint8_t {
    TitleId,
    ProductCode,
    LongName,
    ShortName
};

enum class FolderPlacement : std::uint8_t {
    MixedAlphabetically,
    FoldersFirst
};

struct TitleMetadata {
    std::string iconPath; // Installed title meta/iconTex.tga, loaded with SDL_image.
    // Optional icon extracted from a WUHB bundle. Shared to avoid copying image
    // bytes through every preview frame; renderer decodes it lazily.
    std::shared_ptr<const std::vector<std::uint8_t>> iconData;
    std::string productCode;
    std::string longName;
    std::string shortName;
    std::string publisher;
};

enum class SystemLanguage : std::uint8_t {
    Japanese = 0,
    English = 1,
    French = 2,
    German = 3,
    Italian = 4,
    Spanish = 5,
    ChineseSimplified = 6,
    Korean = 7,
    Dutch = 8,
    Portuguese = 9,
    Russian = 10,
    ChineseTraditional = 11
};

struct LocalizedTitleMetadata {
    std::string productCode;
    std::array<std::string, 12> longNames{};
    std::array<std::string, 12> shortNames{};
    std::array<std::string, 12> publishers{};
};

// Selects the system-language entry, falling back independently to English.
// The caller is responsible for reading meta.xml and mapping language tags to indices.
TitleMetadata ResolveTitleMetadata(const LocalizedTitleMetadata& source,
                                    SystemLanguage language);

using MetadataCatalog = std::unordered_map<std::uint64_t, TitleMetadata>;

struct SortOptions {
    SortField field{SortField::LongName};
    FolderPlacement folderPlacement{FolderPlacement::FoldersFirst};
    bool sortFolderContents{true};
    bool sortFoldersByContents{false}; // Reserved; not implemented because recursive/nested semantics are unproven.
    std::vector<std::uint32_t> excludedPages; // 1-based page numbers.
};

struct SlotAssignment {
    std::size_t destinationIndex{};
    std::optional<std::size_t> sourceIndex; // nullopt means the destination becomes empty in the planned layout.
};

struct Move {
    std::optional<std::uint32_t> folderId; // nullopt = Home Menu.
    std::size_t sourceIndex{};
    std::size_t destinationIndex{};
    std::uint64_t itemId{};
    ItemKind kind{ItemKind::Empty};
};

struct PlannedFolder {
    std::uint32_t folderId{};
    std::array<SlotAssignment, kFolderSlotCount> assignments{};
};

struct PlanResult {
    bool valid{};
    std::array<SlotAssignment, kMainSlotCount> mainAssignments{};
    std::vector<PlannedFolder> folders;
    std::vector<Move> moves;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

ParseResult ParseSave(std::span<const std::uint8_t> bytes);
PlanResult BuildDryRunPlan(const SaveModel& model,
                           const MetadataCatalog& metadata,
                           const SortOptions& options);

std::optional<std::vector<std::uint32_t>> ParseExcludedPages(
    std::string_view expression,
    std::uint32_t maximumPage,
    std::string& error);

std::vector<std::uint32_t> MakeSortKey(std::string_view utf8);

std::string_view ToString(ItemKind kind) noexcept;
std::string_view ToString(SortField field) noexcept;

} // namespace titles_sorter
