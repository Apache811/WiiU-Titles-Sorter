#include "platform/wiiu/WiiUInstalledMetadataProvider.hpp"
#include "McpMock.hpp"
#include "AcpMock.hpp"

#include <algorithm>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace titles_sorter;
namespace fs = std::filesystem;

namespace {
template <std::size_t N>
void CopyTestString(char (&destination)[N], const char* source) {
    static_assert(N > 0U);
    const std::size_t length = std::min(std::strlen(source), N - 1U);
    std::memcpy(destination, source, length);
    destination[length] = '\0';
}

MCPTitleListType MakeTitle(std::uint64_t titleId, const char* path) {
    MCPTitleListType title{};
    title.titleId = titleId;
    const std::size_t pathLength = std::strlen(path);
    assert(pathLength < sizeof(title.path));
    std::memcpy(title.path, path, pathLength);
    title.path[pathLength] = '\0';
    return title;
}

MCPTitleListType MakeTitleWithUnterminatedPath(std::uint64_t titleId) {
    MCPTitleListType title{};
    title.titleId = titleId;
    std::memset(title.path, 'x', sizeof(title.path));
    return title;
}

void TestProviderIncrementalLoad() {
    const fs::path root = fs::path("provider_test_data");
    const fs::path metaDir = root / "title_a" / "meta";
    fs::remove_all(root);
    fs::create_directories(metaDir);
    {
        std::ofstream xml(metaDir / "meta.xml", std::ios::binary);
        xml << R"xml(<?xml version="1.0"?>
<menu>
  <product_code>WUP-P-TEST</product_code>
  <longname_en>English Test Title</longname_en>
  <longname_it>Titolo di Test</longname_it>
  <shortname_en>Test</shortname_en>
  <publisher_en>Test Publisher</publisher_en>
</menu>)xml";
        assert(xml.good());
    }
    {
        std::ofstream icon(metaDir / "iconTex.tga", std::ios::binary);
        const char marker[] = "test-icon";
        icon.write(marker, static_cast<std::streamsize>(sizeof(marker) - 1U));
        assert(icon.good());
    }

    // One valid title and one missing meta.xml: the latter must warn, not abort.
    constexpr std::uint64_t acpFallbackId = 0x0005000098765432ULL;
    ACPMetaXml acpFallback{};
    CopyTestString(acpFallback.longname_en, "ACP &amp; Fallback &apos;Title&apos;");
    CopyTestString(acpFallback.shortname_en, "ACP Fallback");
    CopyTestString(acpFallback.product_code, "WUP-P-ACPF");
    SetMockAcpMetadata(acpFallbackId, acpFallback);

    SetMockMcpTitles({
        MakeTitle(0x0005000012345678ULL, "provider_test_data/title_a"),
        MakeTitle(acpFallbackId, "provider_test_data/missing_title"),
        MakeTitleWithUnterminatedPath(0x0005000011112222ULL),
        MakeTitle(0x0005000F00000001ULL, "provider_test_data/homebrew") // Synthetic entry must not be enumerated here.
    });

    WiiUInstalledMetadataProvider provider;
    std::string error;
    assert(provider.Begin(error));
    assert(error.empty());
    assert(provider.IsLoading());
    assert(provider.Progress().total == 2U);

    const MetadataLoadProgress before = provider.Progress();
    assert(provider.LoadStep(error));
    const MetadataLoadProgress afterOne = provider.Progress();
    assert(afterOne.processed == before.processed + 1U);
    assert(afterOne.state == MetadataLoadState::Loading);
    assert(provider.Catalog().size() == 1U);
    const TitleMetadata& metadata = provider.Catalog().at(0x0005000012345678ULL);
    assert(metadata.productCode == "WUP-P-TEST");
    assert(metadata.longName == "English Test Title");
    assert(metadata.shortName == "Test");
    assert(metadata.publisher == "Test Publisher");
    assert(metadata.iconPath == "provider_test_data/title_a/meta/iconTex.tga");

    assert(provider.LoadStep(error));
    assert(provider.IsComplete());
    assert(provider.Catalog().at(acpFallbackId).longName == "ACP & Fallback 'Title'");
    assert(provider.Catalog().at(acpFallbackId).productCode == "WUP-P-ACPF");
    assert(provider.IsComplete());
    assert(provider.Progress().processed == 2U);
    assert(provider.Progress().warningCount == 2U);
    assert(provider.Warnings().size() == 2U);

    // Synthetic Homebrew on Menu titles are resolved via ACP, not their
    // virtual /custom/... MCP path. The plugin supplies a generic Product Code;
    // the provider deliberately treats it as missing rather than app-specific.
    constexpr std::uint64_t homebrewId = 0x0005000F00012345ULL;
    ACPMetaXml acpMetadata{};
    CopyTestString(acpMetadata.longname_en, "Homebrew Example");
    CopyTestString(acpMetadata.shortname_en, "HB Example");
    CopyTestString(acpMetadata.publisher_en, "Homebrew Author");
    CopyTestString(acpMetadata.product_code, "WUP-P-HBLD");
    SetMockAcpMetadata(homebrewId, acpMetadata);

    assert(provider.AddHomebrewTitleMetadata(homebrewId, error));
    const TitleMetadata& homebrew = provider.Catalog().at(homebrewId);
    assert(homebrew.longName == "Homebrew Example");
    assert(homebrew.shortName == "HB Example");
    assert(homebrew.publisher == "Homebrew Author");
    assert(homebrew.productCode.empty());
    assert(homebrew.iconPath ==
           "storage_mlc01:/sys/title/0005000F/00012345/meta/iconTex.tga");

    assert(!provider.AddHomebrewTitleMetadata(0x0005000012345678ULL, error));
    assert(!provider.AddHomebrewTitleMetadata(0x0005000F00099999ULL, error));
    assert(provider.Warnings().size() == 3U);
    ResetMockAcpMetadata();

    provider.Reset();
    assert(provider.Progress().state == MetadataLoadState::Idle);
    assert(provider.Catalog().empty());
    ResetMockMcpTitles();
    fs::remove_all(root);
}

void TestRequestedSystemTitleMetadata() {
    const fs::path root = fs::path("provider_system_test_data");
    const fs::path metaDir = root / "mii_maker" / "meta";
    fs::remove_all(root);
    fs::create_directories(metaDir);
    {
        std::ofstream xml(metaDir / "meta.xml", std::ios::binary);
        xml << R"xml(<?xml version="1.0"?>
<menu>
  <product_code>WUP-N-HAAP</product_code>
  <longname_en>Mii Maker</longname_en>
  <shortname_en>Mii Maker</shortname_en>
  <publisher_en>Nintendo</publisher_en>
</menu>)xml";
        assert(xml.good());
    }

    constexpr std::uint64_t systemTitleId = 0x000500101004A200ULL;
    constexpr std::uint64_t unrelatedGameId = 0x0005000012345678ULL;
    SetMockMcpTitles({
        MakeTitle(systemTitleId, "provider_system_test_data/mii_maker"),
        MakeTitle(unrelatedGameId, "provider_test_data/title_a")
    });

    const std::array<std::uint64_t, 1> requested{systemTitleId};
    WiiUInstalledMetadataProvider provider;
    std::string error;
    assert(provider.Begin(requested, error));
    assert(provider.Progress().total == 1U);
    while (provider.IsLoading()) assert(provider.LoadStep(error));
    assert(provider.IsComplete());
    assert(provider.Catalog().size() == 1U);
    const TitleMetadata& metadata = provider.Catalog().at(systemTitleId);
    assert(metadata.longName == "Mii Maker");
    assert(metadata.productCode == "WUP-N-HAAP");
    assert(metadata.iconPath == "provider_system_test_data/mii_maker/meta/iconTex.tga");

    provider.Reset();
    ResetMockMcpTitles();
    fs::remove_all(root);
}

void TestSpecialChannelFallbackMetadata() {
    SetMockMcpTitles({});
    const std::array<std::uint64_t, 2> requested{
        0x0005001010055000ULL,
        0x0005001010004000ULL
    };
    WiiUInstalledMetadataProvider provider;
    std::string error;
    assert(provider.Begin(requested, error));
    assert(provider.IsComplete());
    const TitleMetadata& disc = provider.Catalog().at(requested[0]);
    const TitleMetadata& wii = provider.Catalog().at(requested[1]);
    assert(disc.longName == "Disc");
    assert(disc.shortName == "Disc");
    assert(disc.productCode.empty());
    assert(disc.iconPath.find("10055000") != std::string::npos);
    assert(wii.longName == "Wii");
    assert(wii.shortName == "Wii");
    assert(wii.productCode.empty());
    assert(wii.iconPath.find("10004000") != std::string::npos);
    provider.Reset();
    ResetMockMcpTitles();
}

void TestRejectsExcessiveTitleCount() {
    constexpr std::size_t excessiveCount = 4097U;
    std::vector<MCPTitleListType> titles(excessiveCount);
    for (std::size_t i = 0; i < titles.size(); ++i) {
        titles[i] = MakeTitle(0x0005000000000000ULL + static_cast<std::uint64_t>(i),
                              "/unused/title");
    }
    SetMockMcpTitles(titles);

    WiiUInstalledMetadataProvider provider;
    std::string error;
    assert(!provider.Begin(error));
    assert(error.find("unexpectedly large") != std::string::npos);
    assert(provider.Progress().state == MetadataLoadState::Failed);

    ResetMockMcpTitles();
}

void TestEmptyEnumeration() {
    SetMockMcpTitles({});
    WiiUInstalledMetadataProvider provider;
    std::string error;
    assert(provider.Begin(error));
    assert(provider.IsComplete());
    assert(provider.Progress().total == 0U);
    ResetMockMcpTitles();
}

} // namespace

int main() {
    TestProviderIncrementalLoad();
    TestRequestedSystemTitleMetadata();
    TestSpecialChannelFallbackMetadata();
    TestRejectsExcessiveTitleCount();
    TestEmptyEnumeration();
    return 0;
}
