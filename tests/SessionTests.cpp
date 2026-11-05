#include "core/TitlesSorterSession.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <memory>
#include <cstdint>
#include <string>
#include <vector>

static_assert(sizeof(titles_sorter::TitlesSorterSession) <= 32U * 1024U,
              "TitlesSorterSession must remain small enough for the Wii U stack budget.");

namespace {
void WriteBe64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
    for (unsigned i = 0; i < 8U; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> ((7U - i) * 8U));
}
void WriteBe32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4U; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> ((3U - i) * 8U));
}


void TestFolderColourIsCarriedIntoPreview() {
    using namespace titles_sorter;
    std::vector<std::uint8_t> bytes(kExpectedSaveSize, 0U);
    constexpr std::uint32_t folderId = 1U;
    constexpr std::size_t folderSlotOffset = kMainNandOffset;
    WriteBe64(bytes, folderSlotOffset, folderId);
    WriteBe32(bytes, folderSlotOffset + 8U, 0x10U);

    const std::size_t metadataOffset =
        kFolderRegionOffset + kFolderMetadataOffsetInBlock;
    constexpr char folderName[] = "Folder";
    for (std::size_t i = 0; i < sizeof(folderName) - 1U; ++i) {
        bytes[metadataOffset + i * 2U] = 0U;
        bytes[metadataOffset + i * 2U + 1U] =
            static_cast<std::uint8_t>(folderName[i]);
    }
    bytes[metadataOffset + kFolderColorOffsetInMetadata] =
        static_cast<std::uint8_t>(FolderColor::Purple);

    TitlesSorterSession session;
    std::string error;
    assert(session.LoadSave(bytes, error));
    assert(session.BeginPreview(error));
    session.Update();
    assert(session.CurrentScreen() == Screen::Preview);

    const PreviewPage page = session.GetPreviewPage();
    assert(page.slots[0].kind == ItemKind::Folder);
    assert(page.slots[0].label == "Folder");
    assert(page.slots[0].folderColor == FolderColor::Purple);
}

void TestMissingMetadataUsesGenericArtworkPath() {
    using namespace titles_sorter;
    std::vector<std::uint8_t> bytes(kExpectedSaveSize, 0U);
    constexpr std::uint64_t titleId = 0x0005000012345678ULL;
    WriteBe64(bytes, kMainNandOffset, titleId);
    WriteBe32(bytes, kMainNandOffset + 8U, 0x01U);

    TitlesSorterSession session;
    std::string error;
    assert(session.LoadSave(bytes, error));
    MetadataCatalog metadata;
    TitleMetadata incompleteMetadata;
    incompleteMetadata.iconPath = "storage_mlc01:/sys/title/00050000/12345678/meta/iconTex.tga";
    metadata.emplace(titleId, incompleteMetadata);
    session.SetMetadata(&metadata);
    assert(session.BeginPreview(error));
    session.Update();
    const PreviewPage page = session.GetPreviewPage();
    assert(page.slots[0].label == "0005000012345678");
    // Artwork is independent of textual metadata availability.
    assert(page.slots[0].iconPath == incompleteMetadata.iconPath);
}

void TestDiscAndWiiAreSortableAndNotLocked() {
    using namespace titles_sorter;
    std::vector<std::uint8_t> bytes(kExpectedSaveSize, 0U);
    constexpr std::uint64_t discId = 0x0005000012345678ULL;
    constexpr std::uint64_t wiiId = 0x000500101000400AULL;
    constexpr std::uint64_t gameId = 0x0005000011111111ULL;
    WriteBe64(bytes, kMainNandOffset, discId);
    WriteBe32(bytes, kMainNandOffset + 8U, 0x05U);
    WriteBe64(bytes, kMainNandOffset + kRecordSize, wiiId);
    WriteBe32(bytes, kMainNandOffset + kRecordSize + 8U, 0x09U);
    WriteBe64(bytes, kMainNandOffset + 2U * kRecordSize, gameId);
    WriteBe32(bytes, kMainNandOffset + 2U * kRecordSize + 8U, 0x01U);

    TitlesSorterSession session;
    std::string error;
    assert(session.LoadSave(bytes, error));
    MetadataCatalog metadata;
    TitleMetadata disc; disc.longName = "Zebra Disc";
    TitleMetadata wii; wii.longName = "Alpha Wii";
    TitleMetadata game; game.longName = "Middle Game";
    metadata.emplace(discId, disc);
    metadata.emplace(wiiId, wii);
    metadata.emplace(gameId, game);
    session.SetMetadata(&metadata);
    session.Options().field = SortField::LongName;
    session.Options().folderPlacement = FolderPlacement::MixedAlphabetically;
    session.Options().sortFolderContents = false;
    assert(session.BeginPreview(error));
    session.Update();
    assert(session.CurrentScreen() == Screen::Preview);

    const PreviewPage page = session.GetPreviewPage();
    assert(page.slots[0].titleId == wiiId);
    assert(page.slots[1].titleId == gameId);
    assert(page.slots[2].titleId == discId);
    assert(!page.slots[0].pinned);
    assert(!page.slots[2].pinned);
}

void TestSpecialTitleMetadataIsUsedForPreview() {
    using namespace titles_sorter;
    std::vector<std::uint8_t> bytes(kExpectedSaveSize, 0U);
    constexpr std::uint64_t vwiiTitleId = 0x000500101000400AULL;
    WriteBe64(bytes, kMainNandOffset, vwiiTitleId);
    WriteBe32(bytes, kMainNandOffset + 8U, 0x09U);

    TitlesSorterSession session;
    std::string error;
    assert(session.LoadSave(bytes, error));
    MetadataCatalog metadata;
    TitleMetadata systemMetadata;
    systemMetadata.longName = "Wii Menu";
    systemMetadata.iconPath = "storage_mlc01:/sys/title/00050010/1000400A/meta/iconTex.tga";
    systemMetadata.iconData = std::make_shared<const std::vector<std::uint8_t>>(
        std::vector<std::uint8_t>{0x54U, 0x47U, 0x41U});
    metadata.emplace(vwiiTitleId, systemMetadata);
    session.SetMetadata(&metadata);
    assert(session.BeginPreview(error));
    session.Update();
    const PreviewPage page = session.GetPreviewPage();
    assert(page.slots[0].kind == ItemKind::Vwii);
    assert(page.slots[0].label == "Wii Menu");
    assert(page.slots[0].iconPath == systemMetadata.iconPath);
    assert(page.slots[0].iconData == systemMetadata.iconData);
}

}

int main() {
    TestDiscAndWiiAreSortableAndNotLocked();
    TestSpecialTitleMetadataIsUsedForPreview();
    TestMissingMetadataUsesGenericArtworkPath();
    TestFolderColourIsCarriedIntoPreview();

    std::vector<std::uint8_t> bytes(titles_sorter::kExpectedSaveSize, 0U);
    constexpr std::uint64_t highId = 0x0005000012345600ULL;
    constexpr std::uint64_t lowId = 0x0005000010000000ULL;
    // Two titles on page 2, deliberately in reverse Title ID order.
    WriteBe64(bytes, titles_sorter::kMainNandOffset + 15U * titles_sorter::kRecordSize, highId);
    WriteBe32(bytes, titles_sorter::kMainNandOffset + 15U * titles_sorter::kRecordSize + 8U, 0x01U);
    WriteBe64(bytes, titles_sorter::kMainNandOffset + 16U * titles_sorter::kRecordSize, lowId);
    WriteBe32(bytes, titles_sorter::kMainNandOffset + 16U * titles_sorter::kRecordSize + 8U, 0x01U);

    titles_sorter::TitlesSorterSession session;
    std::string error;
    assert(session.LoadSave(bytes, error));
    assert(session.CurrentScreen() == titles_sorter::Screen::Settings);
    assert(session.Model() != nullptr);
    assert(session.Plan() == nullptr);

    titles_sorter::MetadataCatalog metadata;
    titles_sorter::TitleMetadata highMetadata;
    highMetadata.longName = "Zeta";
    highMetadata.productCode = "AAA";
    highMetadata.iconPath = "test/system/iconTex.tga";
    metadata.emplace(highId, highMetadata);
    titles_sorter::TitleMetadata lowMetadata;
    lowMetadata.longName = "Alpha";
    lowMetadata.productCode = "ZZZ";
    metadata.emplace(lowId, lowMetadata);
    session.SetMetadata(&metadata);

    session.Options().field = titles_sorter::SortField::TitleId;
    session.Options().excludedPages = {1U, 3U};
    assert(session.BeginPreview(error));
    assert(session.CurrentScreen() == titles_sorter::Screen::GeneratingPreview);
    assert(session.Plan() == nullptr);

    session.Update();
    assert(session.CurrentScreen() == titles_sorter::Screen::Preview);
    assert(session.Plan() != nullptr && session.Plan()->valid);
    assert(session.PreviewPageNumber() == 1U);
    assert(session.PreviewPageCount() == 20U);

    // Excluded page 1 is unchanged.
    const auto firstPage = session.GetPreviewPage();
    for (std::size_t i = 0; i < titles_sorter::kPageCapacity; ++i)
        assert(!firstPage.slots[i].occupied);

    session.NextPreviewPage();
    assert(session.PreviewPageNumber() == 2U);
    const auto secondPage = session.GetPreviewPage();
    assert(secondPage.slots[0].occupied);
    assert(secondPage.slots[0].titleId == lowId);
    assert(secondPage.slots[0].label == "0005000010000000");
    assert(secondPage.slots[1].titleId == highId);
    assert(secondPage.slots[1].label == "0005000012345600");

    session.PreviousPreviewPage();
    assert(session.PreviewPageNumber() == 1U);
    session.PreviousPreviewPage();
    assert(session.PreviewPageNumber() == 20U);

    session.ReturnToSettings();
    assert(session.CurrentScreen() == titles_sorter::Screen::Settings);
    const auto* cachedPlan = session.Plan();
    assert(cachedPlan != nullptr); // Returning does not discard the dry-run result.

    // Unchanged settings reuse the existing plan without entering the generation state.
    assert(session.BeginPreview(error));
    assert(session.CurrentScreen() == titles_sorter::Screen::Preview);
    assert(session.Plan() == cachedPlan);
    assert(session.PreviewPageNumber() == 1U);

    session.ReturnToSettings();
    session.Options().field = titles_sorter::SortField::ProductCode;
    assert(session.BeginPreview(error));
    assert(session.CurrentScreen() == titles_sorter::Screen::GeneratingPreview);
    session.Update();
    assert(session.CurrentScreen() == titles_sorter::Screen::Preview);
    session.SetPreviewPage(2U);
    const auto productCodePage = session.GetPreviewPage();
    assert(productCodePage.slots[0].titleId == highId);
    assert(productCodePage.slots[0].label == "AAA");
    assert(productCodePage.slots[1].titleId == lowId);
    assert(productCodePage.slots[1].label == "ZZZ");

    session.ReturnToSettings();
    session.Options().field = titles_sorter::SortField::LongName;
    assert(session.BeginPreview(error));
    session.Update();
    session.SetPreviewPage(2U);
    const auto longNamePage = session.GetPreviewPage();
    assert(longNamePage.slots[0].titleId == lowId);
    assert(longNamePage.slots[0].label == "Alpha");
    assert(longNamePage.slots[1].titleId == highId);
    assert(longNamePage.slots[1].label == "Zeta");

    session.ReturnToSettings();
    session.Options().folderPlacement = titles_sorter::FolderPlacement::MixedAlphabetically;
    assert(session.BeginPreview(error));
    assert(session.CurrentScreen() == titles_sorter::Screen::GeneratingPreview);
    session.Update();
    assert(session.CurrentScreen() == titles_sorter::Screen::Preview);
    assert(session.Plan() != nullptr && session.Plan()->valid);

    session.ReturnToSettings();
    session.Options().sortFoldersByContents = true;
    assert(!session.BeginPreview(error));
    assert(!error.empty());
    assert(session.CurrentScreen() == titles_sorter::Screen::Settings);

    const std::vector<std::uint8_t> invalid{0U, 1U, 2U};
    assert(!session.LoadSave(invalid, error));
    assert(session.CurrentScreen() == titles_sorter::Screen::Error);
    assert(session.Model() == nullptr);
    assert(session.Plan() == nullptr);
    return 0;
}
