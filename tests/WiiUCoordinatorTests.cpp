#include "platform/wiiu/WiiUTitlesSorterCoordinator.hpp"
#include "McpMock.hpp"
#include "AcpMock.hpp"

#include <algorithm>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

static_assert(sizeof(titles_sorter::WiiUTitlesSorterCoordinator) <= 32U * 1024U,
              "WiiUTitlesSorterCoordinator must remain small enough for the Wii U stack budget.");

namespace {
template <std::size_t N>
void CopyTestString(char (&destination)[N], const char* source) {
    static_assert(N > 0U);
    const std::size_t length = std::min(std::strlen(source), N - 1U);
    std::memcpy(destination, source, length);
    destination[length] = '\0';
}
void WriteBe64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
    for (unsigned i = 0; i < 8U; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> ((7U - i) * 8U));
}
void WriteBe32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4U; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> ((3U - i) * 8U));
}
}

int main() {
    constexpr std::uint64_t homebrewId = 0x0005000F00012345ULL;
    std::vector<std::uint8_t> save(titles_sorter::kExpectedSaveSize, 0U);
    WriteBe64(save, titles_sorter::kMainNandOffset, homebrewId);
    WriteBe32(save, titles_sorter::kMainNandOffset + 8U, 0x02U);

    ACPMetaXml metadata{};
    CopyTestString(metadata.longname_en, "Coordinator Homebrew");
    CopyTestString(metadata.shortname_en, "Coordinator");
    CopyTestString(metadata.publisher_en, "Local Author");
    CopyTestString(metadata.product_code, "WUP-P-HBLD");
    SetMockAcpMetadata(homebrewId, metadata);
    ResetMockMcpTitles();

    titles_sorter::WiiUTitlesSorterCoordinator coordinator;
    std::string error;
    assert(coordinator.Begin(save, error));
    assert(coordinator.State() == titles_sorter::CoordinatorState::LoadingHomebrewMetadata);
    assert(coordinator.HomebrewMetadataTotal() == 1U);
    assert(coordinator.IsLoading());

    // Startup preferences are applied after Begin(), because Begin() resets
    // the session. They must survive incremental metadata loading unchanged.
    auto& session = coordinator.Session();
    session.Options().field = titles_sorter::SortField::ProductCode;
    session.Options().folderPlacement = titles_sorter::FolderPlacement::MixedAlphabetically;
    session.Options().sortFolderContents = false;
    session.Options().excludedPages = {1U, 4U};

    coordinator.Update(error);
    assert(coordinator.IsReady());
    assert(coordinator.HomebrewMetadataProcessed() == 1U);
    assert(session.CurrentScreen() == titles_sorter::Screen::Settings);
    assert(session.Options().field == titles_sorter::SortField::ProductCode);
    assert(session.Options().folderPlacement == titles_sorter::FolderPlacement::MixedAlphabetically);
    assert(!session.Options().sortFolderContents);
    assert((session.Options().excludedPages == std::vector<std::uint32_t>{1U, 4U}));

    session.Options().field = titles_sorter::SortField::LongName;
    assert(session.BeginPreview(error));
    session.Update();
    assert(session.CurrentScreen() == titles_sorter::Screen::Preview);
    const auto page = session.GetPreviewPage();
    assert(page.slots[0].occupied);
    assert(page.slots[0].titleId == homebrewId);
    assert(page.slots[0].label == "Coordinator Homebrew");

    coordinator.Reset();
    assert(coordinator.State() == titles_sorter::CoordinatorState::Idle);
    ResetMockAcpMetadata();
    return 0;
}
