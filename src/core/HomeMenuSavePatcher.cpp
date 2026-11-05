#include "core/HomeMenuSavePatcher.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace titles_sorter {
namespace {

constexpr std::size_t kMainNandOffset = 0x0000U;
constexpr std::size_t kMainUsbOffset = 0x12C0U;
constexpr std::size_t kFolderRegionOffset = 0x2D24U;
constexpr std::size_t kFolderStride = 0x7B8U;
constexpr std::size_t kFolderNandOffset = 0x000U;
constexpr std::size_t kFolderUsbOffset = 0x3C0U;

static_assert(kMainNandOffset + kMainSlotCount * kRecordSize == kMainUsbOffset);
static_assert(kMainUsbOffset + kMainSlotCount * kRecordSize == kOpaqueRegionOffset);
static_assert(kFolderNandOffset + kFolderSlotCount * kRecordSize == kFolderUsbOffset);
static_assert(kFolderUsbOffset + kFolderSlotCount * kRecordSize == kFolderMetadataOffsetInBlock);
static_assert(kFolderRegionOffset + kFolderCount * kFolderStride == kExpectedSaveSize);

struct SlotBanks {
    std::size_t nand{};
    std::size_t usb{};
};

SlotBanks MainSlotBanks(std::size_t index) noexcept {
    return {kMainNandOffset + index * kRecordSize,
            kMainUsbOffset + index * kRecordSize};
}

SlotBanks FolderSlotBanks(std::uint32_t folderId, std::size_t index) noexcept {
    const std::size_t block = kFolderRegionOffset +
        (static_cast<std::size_t>(folderId) - 1U) * kFolderStride;
    return {block + kFolderNandOffset + index * kRecordSize,
            block + kFolderUsbOffset + index * kRecordSize};
}

bool SameSlot(const Slot& lhs, const Slot& rhs) noexcept {
    return lhs.nand.bytes == rhs.nand.bytes && lhs.usb.bytes == rhs.usb.bytes &&
        lhs.kind == rhs.kind && lhs.titleId == rhs.titleId &&
        lhs.folderId == rhs.folderId && lhs.occupied == rhs.occupied;
}

bool ApplyAssignments(const std::vector<std::uint8_t>& original,
                      std::vector<std::uint8_t>& output,
                      const SlotAssignment* assignments,
                      std::size_t count,
                      const Slot* slots,
                      const std::array<SlotBanks, kMainSlotCount>* mainBanks,
                      std::uint32_t folderId,
                      bool allowFolderRecords,
                      std::string_view label,
                      std::string& error) {
    if (count > kMainSlotCount) {
        error = std::string(label) + ": assignment count exceeds the supported slot capacity.";
        return false;
    }

    // All assignment sets are bounded by the Home Menu's maximum slot count.
    // Fixed storage avoids several short-lived heap allocations during Apply.
    std::array<bool, kMainSlotCount> destinations{};
    std::array<bool, kMainSlotCount> usedSources{};
    std::array<std::size_t, kMainSlotCount> emptyDestinations{};
    std::array<std::size_t, kMainSlotCount> unusedSources{};
    std::size_t emptyDestinationCount = 0U;
    std::size_t unusedSourceCount = 0U;

    for (std::size_t i = 0; i < count; ++i) {
        const SlotAssignment& assignment = assignments[i];
        if (assignment.destinationIndex >= count || destinations[assignment.destinationIndex]) {
            error = std::string(label) + ": duplicate or out-of-range destination.";
            return false;
        }
        destinations[assignment.destinationIndex] = true;
        if (!assignment.sourceIndex.has_value()) {
            emptyDestinations[emptyDestinationCount++] = assignment.destinationIndex;
            continue;
        }
        const std::size_t source = *assignment.sourceIndex;
        if (source >= count || usedSources[source]) {
            error = std::string(label) + ": duplicate or out-of-range source.";
            return false;
        }
        usedSources[source] = true;
    }

    for (std::size_t i = 0; i < count; ++i) {
        if (!usedSources[i]) {
            if (slots[i].occupied) {
                error = std::string(label) + ": plan would discard an occupied source slot.";
                return false;
            }
            unusedSources[unusedSourceCount++] = i;
        }
    }
    if (unusedSourceCount != emptyDestinationCount) {
        error = std::string(label) + ": empty-slot permutation is inconsistent.";
        return false;
    }

    auto banksFor = [&](std::size_t index) noexcept -> SlotBanks {
        return mainBanks != nullptr ? (*mainBanks)[index] : FolderSlotBanks(folderId, index);
    };

    for (std::size_t i = 0; i < count; ++i) {
        const SlotAssignment& assignment = assignments[i];
        if (!assignment.sourceIndex.has_value()) continue;
        const std::size_t source = *assignment.sourceIndex;
        if (source != assignment.destinationIndex) {
            const ItemKind sourceKind = slots[source].kind;
            const ItemKind destinationKind = slots[assignment.destinationIndex].kind;
            const bool sourceMovable = sourceKind == ItemKind::Title ||
                sourceKind == ItemKind::Disc || sourceKind == ItemKind::Vwii ||
                (allowFolderRecords && sourceKind == ItemKind::Folder);
            const bool destinationMovable = destinationKind == ItemKind::Empty ||
                destinationKind == ItemKind::Title || destinationKind == ItemKind::Disc ||
                destinationKind == ItemKind::Vwii ||
                (allowFolderRecords && destinationKind == ItemKind::Folder);
            if (!sourceMovable || !destinationMovable) {
                error = std::string(label) + ": plan attempts to move through a pinned/unknown slot.";
                return false;
            }
        }
        const SlotBanks from = banksFor(source);
        const SlotBanks to = banksFor(assignment.destinationIndex);
        std::copy_n(original.data() + from.nand, kRecordSize, output.data() + to.nand);
        std::copy_n(original.data() + from.usb, kRecordSize, output.data() + to.usb);
    }

    // BuildAssignments represents vacated destinations with nullopt. Move the
    // original empty pairs into those destinations instead of zeroing them, so
    // non-ID bytes in an otherwise empty record are preserved byte-for-byte.
    for (std::size_t i = 0; i < emptyDestinationCount; ++i) {
        const SlotBanks from = banksFor(unusedSources[i]);
        const SlotBanks to = banksFor(emptyDestinations[i]);
        std::copy_n(original.data() + from.nand, kRecordSize, output.data() + to.nand);
        std::copy_n(original.data() + from.usb, kRecordSize, output.data() + to.usb);
    }
    return true;
}

} // namespace

bool HomeMenuSavePatcher::BuildPatchedSave(const SaveModel& model,
                                           const PlanResult& plan,
                                           std::vector<std::uint8_t>& patchedBytes,
                                           std::string& error) const {
    error.clear();
    patchedBytes.clear();
    if (!plan.valid || !plan.errors.empty()) {
        error = "Cannot patch a save from an invalid preview plan.";
        return false;
    }
    if (model.originalBytes.size() != kExpectedSaveSize) {
        error = "The source model does not contain a complete supported save image.";
        return false;
    }

    {
        // Re-parse the exact source bytes and reject stale or inconsistent models.
        const ParseResult sourceParse = ParseSave(model.originalBytes);
        if (!sourceParse || sourceParse.model->mainSelectionCandidate != model.mainSelectionCandidate) {
            error = "The source model is inconsistent with its original save bytes.";
            return false;
        }
        for (std::size_t i = 0; i < kMainSlotCount; ++i) {
            if (!SameSlot(sourceParse.model->mainSlots[i], model.mainSlots[i])) {
                error = "The source model's Home Menu slots do not match its original bytes.";
                return false;
            }
        }
        for (std::size_t folder = 0; folder < kFolderCount; ++folder) {
            const Folder& parsedFolder = sourceParse.model->folders[folder];
            const Folder& sourceFolder = model.folders[folder];
            if (parsedFolder.metadata != sourceFolder.metadata ||
                parsedFolder.name != sourceFolder.name || parsedFolder.color != sourceFolder.color ||
                parsedFolder.rememberedChildSlotCandidate != sourceFolder.rememberedChildSlotCandidate ||
                parsedFolder.active != sourceFolder.active) {
                error = "The source model's folder metadata does not match its original bytes.";
                return false;
            }
            for (std::size_t slot = 0; slot < kFolderSlotCount; ++slot) {
                if (!SameSlot(parsedFolder.slots[slot], sourceFolder.slots[slot])) {
                    error = "The source model's folder slots do not match its original bytes.";
                    return false;
                }
            }
        }
    }

    patchedBytes = model.originalBytes;
    std::array<SlotBanks, kMainSlotCount> mainBanks{};
    for (std::size_t i = 0; i < mainBanks.size(); ++i) mainBanks[i] = MainSlotBanks(i);
    if (!ApplyAssignments(model.originalBytes, patchedBytes, plan.mainAssignments.data(),
                          plan.mainAssignments.size(), model.mainSlots.data(), &mainBanks,
                          0U, true, "Home Menu", error)) {
        patchedBytes.clear();
        return false;
    }

    std::array<bool, kFolderCount> seenFolders{};
    for (const PlannedFolder& folderPlan : plan.folders) {
        if (folderPlan.folderId == 0U || folderPlan.folderId > kFolderCount ||
            seenFolders[folderPlan.folderId - 1U]) {
            error = "Folder plan contains an invalid or duplicate folder identifier.";
            patchedBytes.clear();
            return false;
        }
        seenFolders[folderPlan.folderId - 1U] = true;
        const Folder& folder = model.folders[folderPlan.folderId - 1U];
        if (!folder.active) {
            error = "Folder plan targets an inactive folder.";
            patchedBytes.clear();
            return false;
        }
        if (!ApplyAssignments(model.originalBytes, patchedBytes,
                              folderPlan.assignments.data(), folderPlan.assignments.size(),
                              folder.slots.data(), nullptr, folderPlan.folderId, false,
                              "Folder " + std::to_string(folderPlan.folderId), error)) {
            patchedBytes.clear();
            return false;
        }
    }

    const ParseResult patchedParse = ParseSave(patchedBytes);
    if (!patchedParse) {
        error = "The patched save failed structural parsing.";
        patchedBytes.clear();
        return false;
    }

    // Only the record banks are writable under the currently understood format.
    // Everything else (folder metadata, selection, opaque regions, etc.) must
    // remain identical to the source image.
    std::vector<bool> writable(model.originalBytes.size(), false);
    for (const SlotBanks& banks : mainBanks) {
        std::fill_n(writable.begin() + static_cast<std::ptrdiff_t>(banks.nand), kRecordSize, true);
        std::fill_n(writable.begin() + static_cast<std::ptrdiff_t>(banks.usb), kRecordSize, true);
    }
    for (std::size_t folderIndex = 0; folderIndex < kFolderCount; ++folderIndex) {
        for (std::size_t slot = 0; slot < kFolderSlotCount; ++slot) {
            const SlotBanks banks = FolderSlotBanks(static_cast<std::uint32_t>(folderIndex + 1U), slot);
            std::fill_n(writable.begin() + static_cast<std::ptrdiff_t>(banks.nand), kRecordSize, true);
            std::fill_n(writable.begin() + static_cast<std::ptrdiff_t>(banks.usb), kRecordSize, true);
        }
    }
    for (std::size_t i = 0; i < model.originalBytes.size(); ++i) {
        if (!writable[i] && patchedBytes[i] != model.originalBytes[i]) {
            error = "The patcher modified bytes outside the recognised slot-record banks.";
            patchedBytes.clear();
            return false;
        }
    }

    // Verify that the parser sees precisely the byte pairs emitted at every
    // Home Menu destination. Folder destinations receive the same global
    // byte-preservation check above and are parsed as part of patchedParse.
    for (std::size_t i = 0; i < kMainSlotCount; ++i) {
        const SlotBanks banks = mainBanks[i];
        if (!std::equal(patchedParse.model->mainSlots[i].nand.bytes.begin(),
                        patchedParse.model->mainSlots[i].nand.bytes.end(),
                        patchedBytes.begin() + static_cast<std::ptrdiff_t>(banks.nand)) ||
            !std::equal(patchedParse.model->mainSlots[i].usb.bytes.begin(),
                        patchedParse.model->mainSlots[i].usb.bytes.end(),
                        patchedBytes.begin() + static_cast<std::ptrdiff_t>(banks.usb))) {
            error = "The patched Home Menu record could not be verified.";
            patchedBytes.clear();
            return false;
        }
    }
    for (std::size_t folder = 0; folder < kFolderCount; ++folder) {
        for (std::size_t slot = 0; slot < kFolderSlotCount; ++slot) {
            const SlotBanks banks = FolderSlotBanks(static_cast<std::uint32_t>(folder + 1U), slot);
            const Slot& parsedSlot = patchedParse.model->folders[folder].slots[slot];
            if (!std::equal(parsedSlot.nand.bytes.begin(), parsedSlot.nand.bytes.end(),
                            patchedBytes.begin() + static_cast<std::ptrdiff_t>(banks.nand)) ||
                !std::equal(parsedSlot.usb.bytes.begin(), parsedSlot.usb.bytes.end(),
                            patchedBytes.begin() + static_cast<std::ptrdiff_t>(banks.usb))) {
                error = "The patched folder record could not be verified.";
                patchedBytes.clear();
                return false;
            }
        }
    }
    return true;
}

} // namespace titles_sorter
