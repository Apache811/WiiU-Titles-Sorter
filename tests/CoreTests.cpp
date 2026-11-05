#include "core/TitlesSorterCore.hpp"
#include "core/WiiUMetadataProvider.hpp"
#include "core/TitlesSorterSession.hpp"

#include <algorithm>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace titles_sorter;

static std::vector<std::uint8_t> ReadFile(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open test save file.");
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

static void TestLocalizedMetadataFallback() {
    LocalizedTitleMetadata source;
    source.productCode = "WUP-P-TEST";
    source.longNames[static_cast<std::size_t>(SystemLanguage::English)] = "English Long Name";
    source.longNames[static_cast<std::size_t>(SystemLanguage::Italian)] = "Nome Lungo";
    source.shortNames[static_cast<std::size_t>(SystemLanguage::English)] = "English Short";
    source.publishers[static_cast<std::size_t>(SystemLanguage::English)] = "Example Publisher";

    const TitleMetadata italian = ResolveTitleMetadata(source, SystemLanguage::Italian);
    assert(italian.longName == "Nome Lungo");
    assert(italian.shortName == "English Short");
    assert(italian.productCode == "WUP-P-TEST");
    assert(italian.publisher == "Example Publisher");

    const TitleMetadata japanese = ResolveTitleMetadata(source, SystemLanguage::Japanese);
    assert(japanese.longName == "English Long Name");
    assert(japanese.shortName == "English Short");
}


static void TestWiiUMetaXmlProvider() {
    constexpr std::string_view xml = R"xml(<?xml version="1.0"?>
<menu>
  <product_code>WUP-P-TEST</product_code>
  <longname_en>English &amp; More</longname_en>
  <longname_it>Nome Italiano</longname_it>
  <shortname_en>Short Name</shortname_en>
  <publisher_en>Example Publisher</publisher_en>
  <publisher_it>Editore di Esempio</publisher_it>
</menu>)xml";
    const MetaXmlResult parsed = ParseWiiUMetaXml(xml);
    assert(parsed.valid);
    assert(parsed.metadata.productCode == "WUP-P-TEST");
    assert(parsed.metadata.longNames[static_cast<std::size_t>(SystemLanguage::English)] == "English & More");
    assert(parsed.metadata.longNames[static_cast<std::size_t>(SystemLanguage::Italian)] == "Nome Italiano");
    assert(parsed.metadata.shortNames[static_cast<std::size_t>(SystemLanguage::English)] == "Short Name");
    assert(parsed.metadata.publishers[static_cast<std::size_t>(SystemLanguage::Italian)] == "Editore di Esempio");

    const TitleMetadata resolved = ResolveTitleMetadata(parsed.metadata, SystemLanguage::Italian);
    assert(resolved.longName == "Nome Italiano");
    assert(resolved.shortName == "Short Name");
    assert(resolved.publisher == "Editore di Esempio");

    assert(ParseSystemLanguage("4") == SystemLanguage::Italian);
    assert(ParseSystemLanguage("es-ES") == SystemLanguage::Spanish);
    assert(!ParseSystemLanguage("unknown").has_value());
    assert(ParseSystemLanguageXml("<system><language>4</language></system>") == SystemLanguage::Italian);
    assert(!ParseSystemLanguageXml("<system><language>99</language></system>").has_value());
    assert(DecodeXmlEntities("Fish &amp; Chips &apos;quoted&apos; &quot;yes&quot; &#x41; &#38;") ==
           "Fish & Chips 'quoted' \"yes\" A &");

    MetadataCatalog catalog;
    std::vector<std::string> warnings;
    constexpr std::uint64_t testTitleId = 0x0005000012345678ULL;
    assert(AddTitleMetadata(testTitleId, xml, SystemLanguage::Italian, catalog, warnings));
    assert(catalog.size() == 1U);
    assert(catalog.at(testTitleId).longName == "Nome Italiano");
    assert(catalog.at(testTitleId).publisher == "Editore di Esempio");
    assert(!AddTitleMetadata(testTitleId + 1U, "<menu>", SystemLanguage::English, catalog, warnings));
    assert(catalog.size() == 1U);
    assert(!warnings.empty());

    const MetaXmlResult empty = ParseWiiUMetaXml("");
    assert(!empty.valid);
    const MetaXmlResult malformed = ParseWiiUMetaXml("<menu>");
    assert(!malformed.valid);
    const MetaXmlResult mismatched = ParseWiiUMetaXml("<menu><title></menu></title>");
    assert(!mismatched.valid);
    const MetaXmlResult commented = ParseWiiUMetaXml("<?xml version=\"1.0\"?><!-- header --><menu><longname_en>Example</longname_en></menu>");
    assert(commented.valid);
    assert(commented.metadata.longNames[static_cast<std::size_t>(SystemLanguage::English)] == "Example");
}

static void TestUnicodeSortKeys() {
    assert(MakeSortKey("Éclair") == MakeSortKey("eclair"));
    assert(MakeSortKey("Ångström") == MakeSortKey("angstrom"));
    assert(MakeSortKey("Æther") == MakeSortKey("aether"));
    assert(MakeSortKey("Straße") == MakeSortKey("strasse"));
    assert(MakeSortKey("ΑΛΦΑ") == MakeSortKey("αλφα"));
    assert(MakeSortKey("МОСКВА") == MakeSortKey("москва"));
    assert(MakeSortKey("Game 10") != MakeSortKey("Game 2"));
}

static void TestExcludedPageParser() {
    std::string error;
    auto parsed = ParseExcludedPages("1-3, 6, 8,3", 20, error);
    assert(parsed.has_value());
    assert(((*parsed) == std::vector<std::uint32_t>{1, 2, 3, 6, 8}));

    auto empty = ParseExcludedPages("", 20, error);
    assert(empty.has_value() && empty->empty());

    auto invalidZero = ParseExcludedPages("0,2", 20, error);
    assert(!invalidZero.has_value());

    auto invalidRange = ParseExcludedPages("5-2", 20, error);
    assert(!invalidRange.has_value());

    auto invalidMax = ParseExcludedPages("21", 20, error);
    assert(!invalidMax.has_value());
}

static void TestSpecialDiscAndVwiiRecordsWithConflictingMediaEntries() {
    std::vector<std::uint8_t> bytes = ReadFile("tests/fixtures/sample_menu_save.dat");
    const auto writeBe64 = [&bytes](std::size_t offset, std::uint64_t value) {
        for (std::size_t i = 0; i < 8U; ++i)
            bytes[offset + i] = static_cast<std::uint8_t>(value >> ((7U - i) * 8U));
    };
    const auto writeBe32 = [&bytes](std::size_t offset, std::uint32_t value) {
        for (std::size_t i = 0; i < 4U; ++i)
            bytes[offset + i] = static_cast<std::uint8_t>(value >> ((3U - i) * 8U));
    };

    constexpr std::uint64_t discId = 0x0005000012345678ULL;
    constexpr std::uint64_t vwiiId = 0x000500101000400AULL;
    writeBe64(kMainNandOffset, discId);
    writeBe32(kMainNandOffset + 8U, 0x05U);
    writeBe64(kMainUsbOffset, discId + 1U);
    writeBe32(kMainUsbOffset + 8U, 0x01U);

    writeBe64(kMainNandOffset + kRecordSize, vwiiId);
    writeBe32(kMainNandOffset + kRecordSize + 8U, 0x09U);
    writeBe64(kMainUsbOffset + kRecordSize, vwiiId + 1U);
    writeBe32(kMainUsbOffset + kRecordSize + 8U, 0x01U);

    const ParseResult parsed = ParseSave(bytes);
    assert(parsed);
    assert(parsed.model->mainSlots[0].kind == ItemKind::Disc);
    assert(parsed.model->mainSlots[0].titleId == discId);
    assert(parsed.model->mainSlots[1].kind == ItemKind::Vwii);
    assert(parsed.model->mainSlots[1].titleId == vwiiId);

    // Disc and Wii channels are movable Home Menu entries and participate in
    // the same selected-field ordering as ordinary titles.
    SaveModel sortable;
    sortable.mainSlots[0].kind = ItemKind::Disc;
    sortable.mainSlots[0].titleId = discId;
    sortable.mainSlots[0].occupied = true;
    sortable.mainSlots[1].kind = ItemKind::Vwii;
    sortable.mainSlots[1].titleId = vwiiId;
    sortable.mainSlots[1].occupied = true;
    sortable.mainSlots[2].kind = ItemKind::Title;
    sortable.mainSlots[2].titleId = 0x0005000011111111ULL;
    sortable.mainSlots[2].occupied = true;

    MetadataCatalog catalog;
    TitleMetadata discMetadata;
    discMetadata.longName = "Zebra Disc";
    catalog.emplace(discId, discMetadata);
    TitleMetadata wiiMetadata;
    wiiMetadata.longName = "Alpha Wii";
    catalog.emplace(vwiiId, wiiMetadata);
    TitleMetadata gameMetadata;
    gameMetadata.longName = "Middle Game";
    catalog.emplace(sortable.mainSlots[2].titleId, gameMetadata);

    SortOptions options;
    options.field = SortField::LongName;
    options.folderPlacement = FolderPlacement::MixedAlphabetically;
    options.sortFolderContents = false;
    const PlanResult plan = BuildDryRunPlan(sortable, catalog, options);
    assert(plan.valid);
    assert(plan.mainAssignments[0].sourceIndex == 1U);
    assert(plan.mainAssignments[1].sourceIndex == 2U);
    assert(plan.mainAssignments[2].sourceIndex == 0U);
    bool movedDisc = false;
    bool movedWii = false;
    for (const Move& move : plan.moves) {
        movedDisc = movedDisc || move.kind == ItemKind::Disc;
        movedWii = movedWii || move.kind == ItemKind::Vwii;
    }
    assert(movedDisc && movedWii);
}

static void TestCanonicalDiscAndVwiiSortFieldsWithoutMetadata() {
    std::vector<std::uint8_t> bytes(kExpectedSaveSize, 0U);
    constexpr std::uint64_t discId = 0x0005001010055000ULL;
    constexpr std::uint64_t vwiiId = 0x0005001010004000ULL;
    constexpr std::uint64_t gameId = 0x0005000011111111ULL;
    const auto writeBe64 = [&bytes](std::size_t offset, std::uint64_t value) {
        for (std::size_t i = 0; i < 8U; ++i)
            bytes[offset + i] = static_cast<std::uint8_t>(value >> ((7U - i) * 8U));
    };
    const auto writeBe32 = [&bytes](std::size_t offset, std::uint32_t value) {
        for (std::size_t i = 0; i < 4U; ++i)
            bytes[offset + i] = static_cast<std::uint8_t>(value >> ((3U - i) * 8U));
    };
    writeBe64(kMainNandOffset, discId);
    writeBe32(kMainNandOffset + 8U, 0x05U);
    writeBe64(kMainNandOffset + kRecordSize, vwiiId);
    writeBe32(kMainNandOffset + kRecordSize + 8U, 0x09U);
    writeBe64(kMainNandOffset + 2U * kRecordSize, gameId);
    writeBe32(kMainNandOffset + 2U * kRecordSize + 8U, 0x01U);

    TitlesSorterSession session;
    std::string error;
    assert(session.LoadSave(bytes, error));
    session.Options().field = SortField::TitleId;
    session.Options().folderPlacement = FolderPlacement::MixedAlphabetically;
    session.Options().sortFolderContents = false;
    assert(session.BeginPreview(error));
    session.Update();
    const PreviewPage page = session.GetPreviewPage();
    assert(page.slots[0].titleId == gameId);
    assert(page.slots[1].titleId == vwiiId);
    assert(page.slots[2].titleId == discId);
    assert(page.slots[1].label == "0005001010004000");
    assert(page.slots[2].label == "0005001010055000");
    assert(!page.slots[1].pinned && !page.slots[2].pinned);

    session.Options().field = SortField::ProductCode;
    assert(session.BeginPreview(error));
    session.Update();
    const PreviewPage productCodePage = session.GetPreviewPage();
    bool sawDisc = false;
    bool sawWii = false;
    for (const PreviewItem& item : productCodePage.slots) {
        if (item.kind == ItemKind::Disc) { sawDisc = true; assert(item.label == "N/A"); }
        if (item.kind == ItemKind::Vwii) { sawWii = true; assert(item.label == "N/A"); }
    }
    assert(sawDisc && sawWii);
}

static void TestParserAndPlanner(const char* savePath) {
    const std::vector<std::uint8_t> bytes = ReadFile(savePath);
    const ParseResult parsed = ParseSave(bytes);
    assert(parsed);
    const SaveModel& model = *parsed.model;
    assert(model.originalBytes.size() == kExpectedSaveSize);
    assert(model.mainSelectionCandidate == bytes[kMainSelectionOffset]);

    std::size_t folderCount = 0;
    for (const Folder& folder : model.folders) {
        if (folder.active) ++folderCount;
    }
    assert(folderCount == 11);
    assert(model.folders[0].name == "Hacks");
    assert(model.folders[4].name == "SNES");
    assert(model.folders[11].name == "TurboGrafx");

    // Folder colour is stored at metadata byte 0x33. Verify all eight
    // experimentally established Wii U colour identifiers are parsed.
    constexpr std::array<FolderColor, 8> expectedColors = {
        FolderColor::Blue, FolderColor::Green, FolderColor::Yellow, FolderColor::Orange,
        FolderColor::Red, FolderColor::Rose, FolderColor::Purple, FolderColor::Grey
    };
    constexpr std::size_t firstFolderColorOffset =
        kFolderRegionOffset + kFolderMetadataOffsetInBlock + kFolderColorOffsetInMetadata;
    for (std::size_t color = 0; color < expectedColors.size(); ++color) {
        std::vector<std::uint8_t> colorBytes = bytes;
        colorBytes[firstFolderColorOffset] = static_cast<std::uint8_t>(color);
        const ParseResult colorParsed = ParseSave(colorBytes);
        assert(colorParsed);
        assert(colorParsed.model->folders[0].color == expectedColors[color]);
    }

    std::size_t mainFolders = 0;
    for (const Slot& slot : model.mainSlots) {
        if (slot.kind == ItemKind::Folder) ++mainFolders;
        assert(!(slot.nand.id != 0 && slot.usb.id != 0));
    }
    assert(mainFolders == 11);

    for (const Folder& folder : model.folders) {
        for (const Slot& slot : folder.slots) {
            assert(!(slot.nand.id != 0 && slot.usb.id != 0));
        }
    }

    std::string error;
    auto exclusions = ParseExcludedPages("1,3-4", 20, error);
    assert(exclusions.has_value());

    MetadataCatalog catalog;
    for (const Slot& slot : model.mainSlots) {
        if (slot.kind == ItemKind::Title) {
            TitleMetadata metadata;
            metadata.longName = "Title " + std::to_string(slot.titleId);
            metadata.shortName = metadata.longName;
            metadata.productCode = "WUP-P-TEST";
            metadata.publisher = "Test Publisher";
            catalog.emplace(slot.titleId, std::move(metadata));
        }
    }

    SortOptions options;
    options.field = SortField::LongName;
    options.folderPlacement = FolderPlacement::FoldersFirst;
    options.sortFolderContents = true;
    options.excludedPages = *exclusions;

    const PlanResult plan = BuildDryRunPlan(model, catalog, options);
    assert(plan.valid);
    assert(plan.mainAssignments.size() == kMainSlotCount);
    assert(!plan.folders.empty());
    for (const PlannedFolder& folderPlan : plan.folders) {
        assert(folderPlan.assignments.size() == kFolderSlotCount);
    }

    // Excluded pages are identity mappings.
    for (std::size_t slot = 0; slot < kMainSlotCount; ++slot) {
        const std::uint32_t page = static_cast<std::uint32_t>(slot / kPageCapacity) + 1;
        if (std::binary_search(exclusions->begin(), exclusions->end(), page)) {
            assert(plan.mainAssignments[slot].destinationIndex == slot);
            assert(plan.mainAssignments[slot].sourceIndex.has_value());
            assert(*plan.mainAssignments[slot].sourceIndex == slot);
        }
    }

    // Folder-first mode groups folders ahead of sortable titles while pinned system records stay put.
    SortOptions folderFirstOptions;
    folderFirstOptions.field = SortField::LongName;
    folderFirstOptions.folderPlacement = FolderPlacement::FoldersFirst;
    folderFirstOptions.sortFolderContents = false;
    const PlanResult folderFirstPlan = BuildDryRunPlan(model, catalog, folderFirstOptions);
    assert(folderFirstPlan.valid);
    bool titleSeen = false;
    for (std::size_t destination = 0; destination < kMainSlotCount; ++destination) {
        const auto source = folderFirstPlan.mainAssignments[destination].sourceIndex;
        if (!source.has_value()) continue;
        const ItemKind kind = model.mainSlots[*source].kind;
        if (kind == ItemKind::Title) {
            titleSeen = true;
        } else if (kind == ItemKind::Folder) {
            assert(!titleSeen);
        }
    }

    // Plan generation must not mutate the source snapshot.
    assert(model.originalBytes == bytes);

    // Unknown records remain anchored; ordinary titles and folders can move.
    for (const Move& move : plan.moves) {
        assert(move.kind != ItemKind::Unknown);
    }

    SortOptions rejected;
    rejected.sortFoldersByContents = true;
    const PlanResult rejectedPlan = BuildDryRunPlan(model, catalog, rejected);
    assert(!rejectedPlan.valid);
    assert(!rejectedPlan.errors.empty());
}

int main(int argc, char** argv) {
    try {
        TestLocalizedMetadataFallback();
        TestWiiUMetaXmlProvider();
        TestUnicodeSortKeys();
        TestExcludedPageParser();
        TestSpecialDiscAndVwiiRecordsWithConflictingMediaEntries();
        TestCanonicalDiscAndVwiiSortFieldsWithoutMetadata();
        for (int i = 1; i < argc; ++i) {
            TestParserAndPlanner(argv[i]);
        }
        if (argc == 1) {
            throw std::runtime_error("Parser/planner integration requires at least one save fixture.");
        }
        std::cout << "All Titles Sorter core tests passed for " << (argc - 1) << " sample(s).\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failure: " << ex.what() << '\n';
        return 1;
    }
}
