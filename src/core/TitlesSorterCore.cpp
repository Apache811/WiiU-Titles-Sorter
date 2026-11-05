#include "core/TitlesSorterCore.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

namespace titles_sorter {
namespace {

constexpr std::size_t kFolderNandBankOffset = 0x000;
constexpr std::size_t kFolderUsbBankOffset = 0x3C0;
constexpr std::size_t kFolderBlockMetadataOffset = 0x780;

std::uint32_t ReadBigEndian32(const std::uint8_t* data) noexcept {
    return (static_cast<std::uint32_t>(data[0]) << 24U) |
           (static_cast<std::uint32_t>(data[1]) << 16U) |
           (static_cast<std::uint32_t>(data[2]) << 8U) |
            static_cast<std::uint32_t>(data[3]);
}

std::uint64_t ReadBigEndian64(const std::uint8_t* data) noexcept {
    return (static_cast<std::uint64_t>(ReadBigEndian32(data)) << 32U) |
            static_cast<std::uint64_t>(ReadBigEndian32(data + 4));
}

RawRecord ReadRecord(std::span<const std::uint8_t> bytes, std::size_t offset) {
    RawRecord record;
    std::copy_n(bytes.data() + offset, kRecordSize, record.bytes.data());
    record.id = ReadBigEndian64(record.bytes.data());
    record.type = ReadBigEndian32(record.bytes.data() + 8);
    record.extra = ReadBigEndian32(record.bytes.data() + 12);
    return record;
}

bool IsEmptyId(std::uint64_t id) noexcept {
    return id == 0;
}

Slot BuildSlot(const RawRecord& nand, const RawRecord& usb) {
    Slot slot;
    slot.nand = nand;
    slot.usb = usb;

    const bool nandOccupied = !IsEmptyId(nand.id);
    const bool usbOccupied = !IsEmptyId(usb.id);

    if (nandOccupied && usbOccupied) {
        // A special Home Menu anchor can coexist with a stale/conflicting record
        // in the other media bank. Prefer the explicitly typed Disc/vWii record
        // for preview identity, but keep the original NAND/USB bytes untouched.
        slot.occupied = true;
        const bool nandDisc = nand.type == 0x05U;
        const bool usbDisc = usb.type == 0x05U;
        const bool nandVwii = nand.type == 0x09U;
        const bool usbVwii = usb.type == 0x09U;
        if ((nandDisc || usbDisc) && !(nandVwii || usbVwii)) {
            slot.kind = ItemKind::Disc;
            slot.titleId = nandDisc ? nand.id : usb.id;
        } else if ((nandVwii || usbVwii) && !(nandDisc || usbDisc)) {
            slot.kind = ItemKind::Vwii;
            slot.titleId = nandVwii ? nand.id : usb.id;
        } else {
            slot.kind = ItemKind::Unknown;
            if (nand.id == usb.id) slot.titleId = nand.id;
        }
        return slot;
    }

    if (!nandOccupied && !usbOccupied) {
        slot.kind = ItemKind::Empty;
        return slot;
    }

    const RawRecord& active = nandOccupied ? nand : usb;
    slot.titleId = active.id;
    slot.occupied = true;

    if (nandOccupied && nand.type == 0x10U) {
        slot.kind = ItemKind::Folder;
        slot.folderId = static_cast<std::uint32_t>(nand.id & 0xFFFFFFFFULL);
    } else if (active.type == 0x05U) {
        slot.kind = ItemKind::Disc;
    } else if (active.type == 0x09U) {
        slot.kind = ItemKind::Vwii;
    } else if (active.type == 0x01U || active.type == 0x02U) {
        slot.kind = ItemKind::Title;
    } else {
        slot.kind = ItemKind::Unknown;
    }
    return slot;
}

void AppendUtf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7FU) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else if (codepoint <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
}

std::string DecodeUtf16BigEndian(const std::uint8_t* data, std::size_t length) {
    std::string result;
    result.reserve(length);
    for (std::size_t i = 0; i + 1 < length; i += 2) {
        const std::uint16_t first =
            static_cast<std::uint16_t>((data[i] << 8U) | data[i + 1]);
        if (first == 0) {
            break;
        }

        std::uint32_t codepoint = first;
        if (first >= 0xD800U && first <= 0xDBFFU && i + 3 < length) {
            const std::uint16_t second =
                static_cast<std::uint16_t>((data[i + 2] << 8U) | data[i + 3]);
            if (second >= 0xDC00U && second <= 0xDFFFU) {
                codepoint = 0x10000U +
                    ((static_cast<std::uint32_t>(first) - 0xD800U) << 10U) +
                    (static_cast<std::uint32_t>(second) - 0xDC00U);
                i += 2;
            } else {
                codepoint = 0xFFFDU;
            }
        } else if (first >= 0xDC00U && first <= 0xDFFFU) {
            codepoint = 0xFFFDU;
        }
        AppendUtf8(result, codepoint);
    }
    return result;
}

std::vector<std::uint32_t> MakeSortKeyInternal(std::string_view value) {
    std::vector<std::uint32_t> key;
    key.reserve(value.size());

    for (std::size_t i = 0; i < value.size();) {
        const auto first = static_cast<std::uint8_t>(value[i]);
        std::uint32_t cp = 0xFFFDU;
        std::size_t length = 1;

        if (first < 0x80U) {
            cp = first;
        } else if ((first & 0xE0U) == 0xC0U && i + 1 < value.size()) {
            const auto b1 = static_cast<std::uint8_t>(value[i + 1]);
            if ((b1 & 0xC0U) == 0x80U) {
                cp = ((first & 0x1FU) << 6U) | (b1 & 0x3FU);
                length = cp >= 0x80U ? 2 : 1;
                if (length == 1) cp = 0xFFFDU;
            }
        } else if ((first & 0xF0U) == 0xE0U && i + 2 < value.size()) {
            const auto b1 = static_cast<std::uint8_t>(value[i + 1]);
            const auto b2 = static_cast<std::uint8_t>(value[i + 2]);
            if ((b1 & 0xC0U) == 0x80U && (b2 & 0xC0U) == 0x80U) {
                const std::uint32_t decoded = ((first & 0x0FU) << 12U) |
                    ((b1 & 0x3FU) << 6U) | (b2 & 0x3FU);
                if (decoded >= 0x800U && !(decoded >= 0xD800U && decoded <= 0xDFFFU)) {
                    cp = decoded;
                    length = 3;
                }
            }
        } else if ((first & 0xF8U) == 0xF0U && i + 3 < value.size()) {
            const auto b1 = static_cast<std::uint8_t>(value[i + 1]);
            const auto b2 = static_cast<std::uint8_t>(value[i + 2]);
            const auto b3 = static_cast<std::uint8_t>(value[i + 3]);
            if ((b1 & 0xC0U) == 0x80U && (b2 & 0xC0U) == 0x80U &&
                (b3 & 0xC0U) == 0x80U) {
                const std::uint32_t decoded = ((first & 0x07U) << 18U) |
                    ((b1 & 0x3FU) << 12U) | ((b2 & 0x3FU) << 6U) | (b3 & 0x3FU);
                if (decoded >= 0x10000U && decoded <= 0x10FFFFU) {
                    cp = decoded;
                    length = 4;
                }
            }
        }
        i += length;

        if (cp >= 'A' && cp <= 'Z') cp += 'a' - 'A';
        else if (cp >= 0x00C0U && cp <= 0x00C5U) cp = 'a';
        else if (cp == 0x00C7U) cp = 'c';
        else if (cp >= 0x00C8U && cp <= 0x00CBU) cp = 'e';
        else if (cp >= 0x00CCU && cp <= 0x00CFU) cp = 'i';
        else if (cp == 0x00D1U) cp = 'n';
        else if (cp >= 0x00D2U && cp <= 0x00D6U) cp = 'o';
        else if (cp >= 0x00D9U && cp <= 0x00DCU) cp = 'u';
        else if (cp == 0x00DDU || cp == 0x0178U) cp = 'y';
        else if (cp >= 0x00E0U && cp <= 0x00E5U) cp = 'a';
        else if (cp == 0x00E7U) cp = 'c';
        else if (cp >= 0x00E8U && cp <= 0x00EBU) cp = 'e';
        else if (cp >= 0x00ECU && cp <= 0x00EFU) cp = 'i';
        else if (cp == 0x00F1U) cp = 'n';
        else if (cp >= 0x00F2U && cp <= 0x00F6U) cp = 'o';
        else if (cp >= 0x00F9U && cp <= 0x00FCU) cp = 'u';
        else if (cp == 0x00FDU || cp == 0x00FFU) cp = 'y';
        else if (cp == 0x00C6U || cp == 0x00E6U) { key.push_back('a'); cp = 'e'; }
        else if (cp == 0x00DFU) { key.push_back('s'); cp = 's'; }
        else if (cp >= 0x0391U && cp <= 0x03A1U) cp += 0x20U;
        else if (cp >= 0x03A3U && cp <= 0x03ABU) cp += 0x20U;
        else if (cp >= 0x0410U && cp <= 0x042FU) cp += 0x20U;
        else if (cp >= 0x0400U && cp <= 0x040FU) cp += 0x50U;

        key.push_back(cp);
    }
    return key;
}

std::vector<std::uint32_t> NormalizeForSort(std::string_view value) {
    return MakeSortKeyInternal(value);
}

std::vector<std::uint32_t> TitleIdSortKey(std::uint64_t titleId) {
    std::ostringstream stream;
    stream << std::hex;
    stream.width(16);
    stream.fill('0');
    stream << titleId;
    return NormalizeForSort(stream.str());
}

std::optional<std::vector<std::uint32_t>> GetSortKey(const Slot& slot,
                                      const std::array<Folder, kFolderCount>& folders,
                                      const MetadataCatalog& metadata,
                                      SortField field) {
    if (slot.kind == ItemKind::Folder) {
        if (field == SortField::LongName || field == SortField::ShortName) {
            const std::uint32_t id = slot.folderId;
            if (id == 0 || id > folders.size()) return std::nullopt;
            const std::string& name = folders[id - 1].name;
            if (!name.empty()) return NormalizeForSort(name);
        }
        return std::nullopt;
    }

    if (slot.kind != ItemKind::Title && slot.kind != ItemKind::Disc &&
        slot.kind != ItemKind::Vwii) return std::nullopt;

    if (field == SortField::TitleId) return TitleIdSortKey(slot.titleId);

    const auto it = metadata.find(slot.titleId);
    if (it != metadata.end()) {
        const TitleMetadata& entry = it->second;
        switch (field) {
            case SortField::ProductCode:
                if (!entry.productCode.empty()) return NormalizeForSort(entry.productCode);
                break;
            case SortField::LongName:
                if (!entry.longName.empty()) return NormalizeForSort(entry.longName);
                break;
            case SortField::ShortName:
                if (!entry.shortName.empty()) return NormalizeForSort(entry.shortName);
                break;
            case SortField::TitleId:
                break;
        }
    }

    // Special Home Menu channels have no real Product Code. Never leave them
    // unkeyed at the end of the list: use their actual Title ID as a stable
    // fallback when the selected metadata field does not exist.
    if (slot.kind == ItemKind::Disc || slot.kind == ItemKind::Vwii) {
        if (field == SortField::LongName || field == SortField::ShortName) {
            const std::string_view fallback = slot.kind == ItemKind::Disc ? "Disc" : "Wii";
            return NormalizeForSort(fallback);
        }
        return TitleIdSortKey(slot.titleId);
    }
    return std::nullopt;
}

template <std::size_t N>
std::vector<SlotAssignment> BuildAssignments(
    const std::array<Slot, N>& slots,
    const std::vector<std::size_t>& eligibleIndices,
    const std::array<Folder, kFolderCount>& folders,
    const MetadataCatalog& metadata,
    const SortOptions& options,
    std::optional<std::uint32_t> folderId,
    std::vector<Move>& moves) {

    struct Entry {
        std::size_t source{};
        const Slot* slot{};
        bool hasKey{};
        std::vector<std::uint32_t> key;
    };

    std::vector<Entry> entries;
    entries.reserve(eligibleIndices.size());

    for (const std::size_t index : eligibleIndices) {
        if (slots[index].occupied) {
            Entry entry;
            entry.source = index;
            entry.slot = &slots[index];
            std::optional<std::vector<std::uint32_t>> key =
                GetSortKey(slots[index], folders, metadata, options.field);
            if (key.has_value()) {
                entry.hasKey = true;
                entry.key = std::move(*key);
            }
            entries.push_back(std::move(entry));
        }
    }

    std::stable_sort(entries.begin(), entries.end(),
        [&options](const Entry& lhs, const Entry& rhs) {
            if (options.folderPlacement == FolderPlacement::FoldersFirst) {
                const bool lhsFolder = lhs.slot->kind == ItemKind::Folder;
                const bool rhsFolder = rhs.slot->kind == ItemKind::Folder;
                if (lhsFolder != rhsFolder) {
                    return lhsFolder;
                }
            }

            if (lhs.hasKey != rhs.hasKey) {
                return lhs.hasKey;
            }
            if (!lhs.hasKey) {
                return false;
            }
            return lhs.key < rhs.key;
        });

    std::vector<SlotAssignment> assignments;
    assignments.reserve(eligibleIndices.size());

    for (std::size_t i = 0; i < eligibleIndices.size(); ++i) {
        const std::size_t destination = eligibleIndices[i];
        const std::optional<std::size_t> source =
            i < entries.size() ? std::optional<std::size_t>(entries[i].source) : std::nullopt;
        assignments.push_back({destination, source});

        if (source.has_value() && *source != destination) {
            const Slot& item = slots[*source];
            moves.push_back({folderId, *source, destination, item.titleId, item.kind});
        }
    }
    return assignments;
}

bool IsPageExcluded(std::uint32_t page, const std::vector<std::uint32_t>& excluded) {
    return std::binary_search(excluded.begin(), excluded.end(), page);
}

} // namespace

ParseResult ParseSave(std::span<const std::uint8_t> bytes) {
    ParseResult result;
    if (bytes.size() != kExpectedSaveSize) {
        result.errors.emplace_back("Unexpected save size. Expected exactly 0x1FC44 bytes.");
        return result;
    }

    result.model = std::make_unique<SaveModel>();
    SaveModel& model = *result.model;
    model.originalBytes.assign(bytes.begin(), bytes.end());
    model.mainSelectionCandidate = bytes[kMainSelectionOffset];

    std::array<RawRecord, kMainSlotCount> mainNand{};
    std::array<RawRecord, kMainSlotCount> mainUsb{};

    for (std::size_t i = 0; i < kMainSlotCount; ++i) {
        mainNand[i] = ReadRecord(bytes, kMainNandOffset + i * kRecordSize);
        mainUsb[i] = ReadRecord(bytes, kMainUsbOffset + i * kRecordSize);
        model.mainSlots[i] = BuildSlot(mainNand[i], mainUsb[i]);

        if (model.mainSlots[i].kind == ItemKind::Unknown) {
            model.warnings.emplace_back("Unknown or colliding record at main slot " +
                                        std::to_string(i) + ".");
        }
        if (model.mainSlots[i].kind == ItemKind::Folder &&
            (model.mainSlots[i].folderId == 0 || model.mainSlots[i].folderId > kFolderCount)) {
            model.warnings.emplace_back("Folder reference out of range at main slot " +
                                        std::to_string(i) + ".");
        }
    }

    for (std::size_t folderIndex = 0; folderIndex < kFolderCount; ++folderIndex) {
        const std::size_t blockStart = kFolderRegionOffset + folderIndex * kFolderStride;
        Folder& folder = model.folders[folderIndex];
        folder.id = static_cast<std::uint32_t>(folderIndex + 1);

        const std::size_t metadataStart = blockStart + kFolderBlockMetadataOffset;
        std::copy_n(bytes.data() + metadataStart, kFolderMetadataSize, folder.metadata.data());
        folder.name = DecodeUtf16BigEndian(folder.metadata.data(), folder.metadata.size());
        // The Wii U stores the folder colour as a byte in the metadata block.
        // Preserve unknown values in the enum representation; the renderer
        // safely falls back to blue if a future firmware uses another value.
        folder.color = static_cast<FolderColor>(
            folder.metadata[kFolderColorOffsetInMetadata]);
        folder.rememberedChildSlotCandidate =
            folder.metadata[kFolderRememberedSlotOffset];

        bool hasContent = false;
        for (std::size_t slotIndex = 0; slotIndex < kFolderSlotCount; ++slotIndex) {
            const RawRecord nand = ReadRecord(
                bytes, blockStart + kFolderNandBankOffset + slotIndex * kRecordSize);
            const RawRecord usb = ReadRecord(
                bytes, blockStart + kFolderUsbBankOffset + slotIndex * kRecordSize);
            folder.slots[slotIndex] = BuildSlot(nand, usb);
            hasContent = hasContent || folder.slots[slotIndex].occupied;
            if (folder.slots[slotIndex].kind == ItemKind::Unknown) {
                model.warnings.emplace_back("Unknown or colliding record in folder " +
                    std::to_string(folder.id) + ", slot " + std::to_string(slotIndex) + ".");
            }
        }
        folder.active = !folder.name.empty() || hasContent;
    }

    return result;
}

PlanResult BuildDryRunPlan(const SaveModel& model,
                           const MetadataCatalog& metadata,
                           const SortOptions& options) {
    PlanResult plan;
    if (options.sortFoldersByContents) {
        plan.errors.emplace_back(
            "Sorting folders by their contents is disabled: recursive/nested-folder semantics are unproven.");
        return plan;
    }

    std::vector<std::uint32_t> excluded = options.excludedPages;
    std::sort(excluded.begin(), excluded.end());
    excluded.erase(std::unique(excluded.begin(), excluded.end()), excluded.end());

    for (const std::uint32_t page : excluded) {
        if (page == 0 || page > kMainSlotCount / kPageCapacity) {
            plan.errors.emplace_back("Excluded page is outside the observed page range.");
            return plan;
        }
    }

    std::vector<std::size_t> eligibleMainSlots;
    eligibleMainSlots.reserve(kMainSlotCount);
    for (std::size_t slotIndex = 0; slotIndex < kMainSlotCount; ++slotIndex) {
        const std::uint32_t page =
            static_cast<std::uint32_t>(slotIndex / kPageCapacity) + 1U;
        if (!IsPageExcluded(page, excluded)) {
            const ItemKind kind = model.mainSlots[slotIndex].kind;
            // Unknown/system records are anchors until their movement semantics are proven.
            const bool validFolderReference =
                kind == ItemKind::Folder &&
                model.mainSlots[slotIndex].folderId > 0 &&
                model.mainSlots[slotIndex].folderId <= kFolderCount &&
                model.folders[model.mainSlots[slotIndex].folderId - 1].active;
            if (kind == ItemKind::Empty || kind == ItemKind::Title ||
                kind == ItemKind::Disc || kind == ItemKind::Vwii || validFolderReference) {
                eligibleMainSlots.push_back(slotIndex);
            }
        }
        plan.mainAssignments[slotIndex] = {slotIndex, slotIndex};
    }

    const std::vector<SlotAssignment> mainAssignments = BuildAssignments(
        model.mainSlots, eligibleMainSlots, model.folders, metadata, options,
        std::nullopt, plan.moves);
    for (const SlotAssignment& assignment : mainAssignments) {
        plan.mainAssignments[assignment.destinationIndex] = assignment;
    }

    if (options.sortFolderContents) {
        for (const Folder& folder : model.folders) {
            if (!folder.active) {
                continue;
            }
            std::vector<std::size_t> indices;
            indices.reserve(kFolderSlotCount);
            for (std::size_t i = 0; i < kFolderSlotCount; ++i) {
                const ItemKind kind = folder.slots[i].kind;
                // Nested folders and unknown records remain anchored. Disc and
                // vWii entries follow the same sorting rules as ordinary titles.
                if (kind == ItemKind::Empty || kind == ItemKind::Title ||
                    kind == ItemKind::Disc || kind == ItemKind::Vwii) {
                    indices.push_back(i);
                }
            }
            PlannedFolder folderPlan;
            folderPlan.folderId = folder.id;
            for (std::size_t i = 0; i < kFolderSlotCount; ++i) {
                folderPlan.assignments[i] = {i, i};
            }
            const std::vector<SlotAssignment> assignments = BuildAssignments(
                folder.slots, indices, model.folders, metadata, options,
                folder.id, plan.moves);
            for (const SlotAssignment& assignment : assignments) {
                folderPlan.assignments[assignment.destinationIndex] = assignment;
            }
            plan.folders.push_back(std::move(folderPlan));
        }
    }

    // Every source index must be assigned at most once per independent container.
    auto validateAssignments = [&plan](const std::vector<SlotAssignment>& assignments,
                                        std::size_t capacity,
                                        std::string_view label) {
        std::vector<bool> seen(capacity, false);
        for (const SlotAssignment& assignment : assignments) {
            if (assignment.destinationIndex >= capacity) {
                plan.errors.emplace_back(std::string(label) + ": destination out of range.");
                continue;
            }
            if (assignment.sourceIndex.has_value()) {
                if (*assignment.sourceIndex >= capacity) {
                    plan.errors.emplace_back(std::string(label) + ": source out of range.");
                } else if (seen[*assignment.sourceIndex]) {
                    plan.errors.emplace_back(std::string(label) + ": duplicate source assignment.");
                } else {
                    seen[*assignment.sourceIndex] = true;
                }
            }
        }
    };

    std::vector<SlotAssignment> mainVector(plan.mainAssignments.begin(), plan.mainAssignments.end());
    validateAssignments(mainVector, kMainSlotCount, "Home Menu");
    for (const PlannedFolder& folder : plan.folders) {
        std::vector<SlotAssignment> assignments(
            folder.assignments.begin(), folder.assignments.end());
        validateAssignments(assignments, kFolderSlotCount,
                            "Folder " + std::to_string(folder.folderId));
    }

    plan.valid = plan.errors.empty();
    if (!plan.valid) {
        plan.moves.clear();
    }
    plan.warnings = model.warnings;
    plan.warnings.emplace_back(
        "Dry-run only: no save bytes were changed; navigation pointers and unknown metadata are untouched.");
    plan.warnings.emplace_back(
        "15 slots per Home Menu page is an experimentally supported hypothesis, not an official format guarantee.");
    plan.warnings.emplace_back(
        "Unknown records and nested folders remain fixed; Disc and Wii entries are sortable like ordinary titles.");
    plan.warnings.emplace_back(
        "Text sorting folds common Latin diacritics and case in Latin, Greek and Cyrillic; full locale-specific collation and Unicode normalization are not implemented.");
    return plan;
}

std::optional<std::vector<std::uint32_t>> ParseExcludedPages(
    std::string_view expression,
    std::uint32_t maximumPage,
    std::string& error) {
    std::vector<std::uint32_t> pages;
    std::size_t position = 0;

    while (position < expression.size()) {
        while (position < expression.size() &&
               (expression[position] == ' ' || expression[position] == '\t')) {
            ++position;
        }
        if (position == expression.size()) break;

        const std::size_t tokenStart = position;
        while (position < expression.size() && expression[position] != ',') {
            ++position;
        }
        std::string token(expression.substr(tokenStart, position - tokenStart));
        const std::size_t dash = token.find('-');

        auto parseNumber = [&error, maximumPage](std::string_view text,
                                                 std::uint32_t& value) -> bool {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
            if (text.empty()) {
                error = "Empty page number in exclusion expression.";
                return false;
            }
            std::uint64_t parsed = 0;
            for (char ch : text) {
                if (ch < '0' || ch > '9') {
                    error = "Invalid character in page exclusion expression.";
                    return false;
                }
                parsed = parsed * 10U + static_cast<unsigned>(ch - '0');
                if (parsed > maximumPage) {
                    error = "Excluded page exceeds the maximum page number.";
                    return false;
                }
            }
            if (parsed == 0) {
                error = "Page numbers are 1-based; zero is invalid.";
                return false;
            }
            value = static_cast<std::uint32_t>(parsed);
            return true;
        };

        if (dash == std::string::npos) {
            std::uint32_t page = 0;
            if (!parseNumber(token, page)) return std::nullopt;
            pages.push_back(page);
        } else {
            if (token.find('-', dash + 1) != std::string::npos) {
                error = "Invalid page range.";
                return std::nullopt;
            }
            std::uint32_t first = 0, last = 0;
            if (!parseNumber(std::string_view(token).substr(0, dash), first) ||
                !parseNumber(std::string_view(token).substr(dash + 1), last)) {
                return std::nullopt;
            }
            if (first > last) {
                error = "Page range start must not exceed its end.";
                return std::nullopt;
            }
            for (std::uint32_t page = first; page <= last; ++page) {
                pages.push_back(page);
            }
        }

        if (position < expression.size()) {
            ++position;
            if (position == expression.size()) {
                error = "Trailing comma in page exclusion expression.";
                return std::nullopt;
            }
        }
    }

    std::sort(pages.begin(), pages.end());
    pages.erase(std::unique(pages.begin(), pages.end()), pages.end());
    return pages;
}

std::vector<std::uint32_t> MakeSortKey(std::string_view utf8) {
    return MakeSortKeyInternal(utf8);
}

TitleMetadata ResolveTitleMetadata(const LocalizedTitleMetadata& source,
                                    SystemLanguage language) {
    const std::size_t languageIndex = static_cast<std::size_t>(language);
    const std::size_t englishIndex = static_cast<std::size_t>(SystemLanguage::English);

    TitleMetadata result;
    result.productCode = source.productCode;
    result.longName = languageIndex < source.longNames.size() &&
        !source.longNames[languageIndex].empty()
        ? source.longNames[languageIndex] : source.longNames[englishIndex];
    result.shortName = languageIndex < source.shortNames.size() &&
        !source.shortNames[languageIndex].empty()
        ? source.shortNames[languageIndex] : source.shortNames[englishIndex];
    result.publisher = languageIndex < source.publishers.size() &&
        !source.publishers[languageIndex].empty()
        ? source.publishers[languageIndex] : source.publishers[englishIndex];
    return result;
}

std::string_view ToString(ItemKind kind) noexcept {
    switch (kind) {
        case ItemKind::Empty: return "Empty";
        case ItemKind::Title: return "Title";
        case ItemKind::Folder: return "Folder";
        case ItemKind::Disc: return "Disc";
        case ItemKind::Vwii: return "vWii";
        case ItemKind::Unknown: return "Unknown";
    }
    return "Unknown";
}

std::string_view ToString(SortField field) noexcept {
    switch (field) {
        case SortField::TitleId: return "Title ID";
        case SortField::ProductCode: return "Product Code";
        case SortField::LongName: return "Long Name";
        case SortField::ShortName: return "Short Name";
    }
    return "Unknown";
}

} // namespace titles_sorter
