#include "core/HomeMenuSavePatcher.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace titles_sorter;

namespace {

void WriteBe64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
    for (std::size_t i = 0; i < 8U; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> ((7U - i) * 8U));
}

void WriteBe32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4U; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> ((3U - i) * 8U));
}

void WriteTitle(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t id) {
    WriteBe64(bytes, offset, id);
    WriteBe32(bytes, offset + 8U, 0x01U);
}

void WriteUtf16Ascii(std::vector<std::uint8_t>& bytes, std::size_t offset,
                     const std::string& value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        bytes[offset + i * 2U] = 0U;
        bytes[offset + i * 2U + 1U] = static_cast<std::uint8_t>(value[i]);
    }
}

std::vector<std::uint8_t> MakeSave() {
    std::vector<std::uint8_t> bytes(kExpectedSaveSize, 0U);
    // Slot zero is empty, but has non-ID bytes that must follow the empty
    // slot to its new destination after two occupied entries move forward.
    bytes[0x0008U] = 0xA5U;
    bytes[0x000FU] = 0x5AU;
    WriteTitle(bytes, 0x0010U, 0x2000000000000002ULL);
    WriteTitle(bytes, 0x0020U, 0x2000000000000001ULL);
    // An unknown record is an anchor and must remain byte-identical.
    WriteBe64(bytes, 0x0040U, 0x2F00000000000001ULL);
    WriteBe32(bytes, 0x0048U, 0x99U);

    const std::size_t folderBlock = 0x2D24U;
    WriteUtf16Ascii(bytes, folderBlock + 0x780U, "Folder");
    WriteTitle(bytes, folderBlock + 0x010U, 0x2000000000000004ULL);
    WriteTitle(bytes, folderBlock + 0x020U, 0x2000000000000003ULL);

    bytes[0x2587U] = 0x17U; // Selection candidate must not be touched.
    bytes[0x2700U] = 0xC3U; // Opaque region sentinel.
    bytes[folderBlock + 0x780U + 50U] = 0xD4U; // Folder metadata sentinel.
    return bytes;
}

void TestPatchPreservesOpaqueBytesAndSortsBothContainers() {
    std::vector<std::uint8_t> original = MakeSave();
    ParseResult parsed = ParseSave(original);
    assert(parsed);

    MetadataCatalog metadata;
    const auto addMetadata = [&metadata](std::uint64_t id, const char* name) {
        TitleMetadata entry;
        entry.longName = name;
        metadata.emplace(id, std::move(entry));
    };
    addMetadata(0x2000000000000002ULL, "Zulu");
    addMetadata(0x2000000000000001ULL, "Alpha");
    addMetadata(0x2000000000000004ULL, "Yankee");
    addMetadata(0x2000000000000003ULL, "Bravo");

    SortOptions options;
    options.field = SortField::LongName;
    options.sortFolderContents = true;
    PlanResult plan = BuildDryRunPlan(*parsed.model, metadata, options);
    assert(plan.valid);

    HomeMenuSavePatcher patcher;
    std::vector<std::uint8_t> patched;
    std::string error;
    assert(patcher.BuildPatchedSave(*parsed.model, plan, patched, error));
    assert(error.empty());
    assert(patched.size() == original.size());

    ParseResult result = ParseSave(patched);
    assert(result);
    assert(result.model->mainSlots[0].titleId == 0x2000000000000001ULL);
    assert(result.model->mainSlots[1].titleId == 0x2000000000000002ULL);
    assert(!result.model->mainSlots[2].occupied);
    assert(result.model->folders[0].slots[0].titleId == 0x2000000000000003ULL);
    assert(result.model->folders[0].slots[1].titleId == 0x2000000000000004ULL);
    assert(!result.model->folders[0].slots[2].occupied);

    assert(patched[0x2587U] == original[0x2587U]);
    assert(patched[0x2700U] == original[0x2700U]);
    assert(patched[0x2D24U + 0x780U + 50U] == original[0x2D24U + 0x780U + 50U]);
    assert(patched[0x0028U] == original[0x0008U]);
    assert(patched[0x002FU] == original[0x000FU]);
    assert(std::equal(patched.begin() + 0x0040, patched.begin() + 0x0050,
                      original.begin() + 0x0040));
    assert(parsed.model->originalBytes == original); // Input model remains immutable.
}

void TestRejectsInvalidPlanAndTamperedModel() {
    std::vector<std::uint8_t> original = MakeSave();
    ParseResult parsed = ParseSave(original);
    assert(parsed);
    MetadataCatalog metadata;
    PlanResult plan = BuildDryRunPlan(*parsed.model, metadata, SortOptions{});
    assert(plan.valid);

    HomeMenuSavePatcher patcher;
    std::vector<std::uint8_t> output;
    std::string error;

    PlanResult invalid = plan;
    invalid.valid = false;
    assert(!patcher.BuildPatchedSave(*parsed.model, invalid, output, error));
    assert(output.empty());

    invalid = plan;
    invalid.mainAssignments[0].sourceIndex = invalid.mainAssignments[1].sourceIndex;
    assert(!patcher.BuildPatchedSave(*parsed.model, invalid, output, error));
    assert(output.empty());

    SaveModel tampered = *parsed.model;
    tampered.mainSlots[0].nand.bytes[0] ^= 0xFFU;
    assert(!patcher.BuildPatchedSave(tampered, plan, output, error));
    assert(output.empty());

    tampered = *parsed.model;
    tampered.mainSlots[1].titleId ^= 1U;
    assert(!patcher.BuildPatchedSave(tampered, plan, output, error));
    assert(output.empty());
}

} // namespace

int main() {
    TestPatchPreservesOpaqueBytesAndSortsBothContainers();
    TestRejectsInvalidPlanAndTamperedModel();
    std::cout << "HomeMenuSavePatcherTests passed.\n";
    return 0;
}
