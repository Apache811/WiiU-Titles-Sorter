#include "core/TitlesSorterSession.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace titles_sorter {
namespace {

bool SameOptions(const SortOptions& left, const SortOptions& right) noexcept {
    return left.field == right.field &&
           left.folderPlacement == right.folderPlacement &&
           left.sortFolderContents == right.sortFolderContents &&
           left.sortFoldersByContents == right.sortFoldersByContents &&
           left.excludedPages == right.excludedPages;
}

std::string FormatTitleId(std::uint64_t id) {
    char buffer[17]{};
    std::snprintf(buffer, sizeof(buffer), "%016llX", static_cast<unsigned long long>(id));
    return buffer;
}

} // namespace

bool TitlesSorterSession::LoadSave(std::span<const std::uint8_t> bytes, std::string& error) {
    error.clear();
    ParseResult parsed = ParseSave(bytes);
    if (!parsed) {
        error = parsed.errors.empty() ? "Unable to parse Home Menu save." : parsed.errors.front();
        errorMessage_ = error;
        screen_ = Screen::Error;
        model_.reset();
        hasModel_ = false;
        hasPlan_ = false;
        return false;
    }

    model_ = std::move(parsed.model);
    hasModel_ = true;
    hasPlan_ = false;
    previewPage_ = 1;
    errorMessage_.clear();
    BuildPreviewLabels();
    screen_ = Screen::Settings;
    return true;
}

void TitlesSorterSession::SetMetadata(const MetadataCatalog* metadata) noexcept {
    if (metadata_ != metadata) {
        metadata_ = metadata;
        hasPlan_ = false;
    }
    if (hasModel_) BuildPreviewLabels();
}

SortOptions& TitlesSorterSession::Options() noexcept { return options_; }
const SortOptions& TitlesSorterSession::Options() const noexcept { return options_; }
Screen TitlesSorterSession::CurrentScreen() const noexcept { return screen_; }
const std::string& TitlesSorterSession::ErrorMessage() const noexcept { return errorMessage_; }
const SaveModel* TitlesSorterSession::Model() const noexcept { return hasModel_ ? model_.get() : nullptr; }
const PlanResult* TitlesSorterSession::Plan() const noexcept { return hasPlan_ ? &plan_ : nullptr; }

bool TitlesSorterSession::BeginPreview(std::string& error) {
    error.clear();
    if (!hasModel_) {
        error = "Home Menu save has not been loaded.";
        errorMessage_ = error;
        screen_ = Screen::Error;
        return false;
    }
    if (options_.sortFoldersByContents) {
        error = "Sorting folders by aggregate contents is not supported.";
        errorMessage_ = error;
        screen_ = Screen::Settings;
        return false;
    }

    previewPage_ = 1;
    BuildPreviewLabels();
    if (hasPlan_ && plannedMetadata_ == metadata_ &&
        SameOptions(options_, plannedOptions_)) {
        errorMessage_.clear();
        screen_ = Screen::Preview;
        return true;
    }

    plan_ = {};
    hasPlan_ = false;
    errorMessage_.clear();
    screen_ = Screen::GeneratingPreview;
    return true;
}

void TitlesSorterSession::Update() {
    if (screen_ != Screen::GeneratingPreview || !hasModel_) return;

    static const MetadataCatalog emptyMetadata;
    const MetadataCatalog& metadata = metadata_ != nullptr ? *metadata_ : emptyMetadata;
    plan_ = BuildDryRunPlan(*model_, metadata, options_);
    if (!plan_.valid) {
        errorMessage_ = plan_.errors.empty() ? "Preview generation failed." : plan_.errors.front();
        screen_ = Screen::Error;
        hasPlan_ = false;
        return;
    }

    hasPlan_ = true;
    plannedOptions_ = options_;
    plannedMetadata_ = metadata_;
    screen_ = Screen::Preview;
}

void TitlesSorterSession::ReturnToSettings() noexcept {
    if (screen_ == Screen::Preview || screen_ == Screen::Error) {
        screen_ = Screen::Settings;
        errorMessage_.clear();
    }
}

void TitlesSorterSession::PreviousPreviewPage(std::uint32_t folderId) noexcept {
    if (screen_ != Screen::Preview) return;
    const std::uint32_t count = PreviewPageCount(folderId);
    previewPage_ = previewPage_ <= 1U ? count : previewPage_ - 1U;
}

void TitlesSorterSession::NextPreviewPage(std::uint32_t folderId) noexcept {
    if (screen_ != Screen::Preview) return;
    const std::uint32_t count = PreviewPageCount(folderId);
    previewPage_ = previewPage_ >= count ? 1U : previewPage_ + 1U;
}

void TitlesSorterSession::ResetPreviewPage() noexcept { previewPage_ = 1U; }

void TitlesSorterSession::SetPreviewPage(std::uint32_t page) noexcept {
    previewPage_ = std::max(1U, std::min(page, PreviewPageCount()));
}

std::uint32_t TitlesSorterSession::PreviewPageNumber() const noexcept { return previewPage_; }
std::uint32_t TitlesSorterSession::PreviewPageCount() const noexcept {
    return static_cast<std::uint32_t>(kMainSlotCount / kPageCapacity);
}

std::uint32_t TitlesSorterSession::PreviewPageCount(std::uint32_t folderId) const noexcept {
    if (folderId == 0U) return PreviewPageCount();
    return static_cast<std::uint32_t>(kFolderSlotCount / kPageCapacity);
}

std::string TitlesSorterSession::FolderName(std::uint32_t folderId) const {
    if (!hasModel_ || folderId == 0U || folderId > model_->folders.size()) return {};
    return model_->folders[folderId - 1U].name;
}

PreviewPage TitlesSorterSession::GetPreviewPage(std::uint32_t folderId) const {
    PreviewPage page;
    page.pageNumber = previewPage_;
    if (!hasModel_ || !hasPlan_ || screen_ != Screen::Preview) return page;

    const SlotAssignment* assignments = plan_.mainAssignments.data();
    std::array<SlotAssignment, kFolderSlotCount> identityAssignments{};
    std::size_t capacity = kMainSlotCount;
    if (folderId != 0U) {
        if (folderId > model_->folders.size()) return page;
        const Folder& folder = model_->folders[folderId - 1U];
        if (!folder.active) return page;
        const auto found = std::find_if(plan_.folders.begin(), plan_.folders.end(),
            [folderId](const PlannedFolder& candidate) { return candidate.folderId == folderId; });
        if (found != plan_.folders.end()) {
            assignments = found->assignments.data();
        } else {
            for (std::size_t i = 0; i < identityAssignments.size(); ++i)
                identityAssignments[i] = {i, i};
            assignments = identityAssignments.data();
        }
        capacity = kFolderSlotCount;
    }

    const std::size_t first = static_cast<std::size_t>(previewPage_ - 1U) * kPageCapacity;
    for (std::size_t local = 0; local < kPageCapacity; ++local) {
        const std::size_t destination = first + local;
        if (destination >= capacity) break;
        const SlotAssignment& assignment = assignments[destination];
        PreviewItem& item = page.slots[local];
        if (!assignment.sourceIndex.has_value()) continue;
        const std::size_t source = *assignment.sourceIndex;
        const Slot& slot = folderId == 0U
            ? model_->mainSlots[source]
            : model_->folders[folderId - 1U].slots[source];
        item.kind = slot.kind;
        item.titleId = slot.titleId;
        item.folderId = slot.folderId;
        item.occupied = slot.occupied;
        if (source == destination) {
            const std::uint32_t sourcePage = static_cast<std::uint32_t>(source / kPageCapacity) + 1U;
            const bool pageExcluded = folderId == 0U &&
                std::find(options_.excludedPages.begin(), options_.excludedPages.end(), sourcePage) != options_.excludedPages.end();
            const bool validFolder = slot.kind == ItemKind::Folder && slot.folderId > 0U &&
                slot.folderId <= kFolderCount && model_->folders[slot.folderId - 1U].active;
            const bool movable = slot.kind == ItemKind::Title || slot.kind == ItemKind::Disc ||
                slot.kind == ItemKind::Vwii || validFolder;
            item.pinned = pageExcluded || (slot.occupied && !movable);
        }
        if (slot.kind == ItemKind::Folder && slot.folderId > 0U && slot.folderId <= model_->folders.size()) {
            const Folder& folder = model_->folders[slot.folderId - 1U];
            item.label = folder.name;
            item.folderColor = folder.color;
        } else {
            // Resolve icon artwork and the selected display value independently.
            // A missing metadata entry must never hide a Title ID or make a
            // special channel look like a generic unknown title.
            const TitleMetadata* titleMetadata = nullptr;
            if (metadata_ != nullptr && slot.titleId != 0U) {
                const auto found = metadata_->find(slot.titleId);
                if (found != metadata_->end()) {
                    titleMetadata = &found->second;
                    item.iconPath = found->second.iconPath;
                    item.iconData = found->second.iconData;
                }
            }

            switch (plannedOptions_.field) {
                case SortField::TitleId:
                    item.label = FormatTitleId(slot.titleId);
                    break;
                case SortField::ProductCode:
                    if (titleMetadata != nullptr && !titleMetadata->productCode.empty())
                        item.label = titleMetadata->productCode;
                    else if (slot.kind == ItemKind::Disc || slot.kind == ItemKind::Vwii)
                        item.label = "N/A";
                    break;
                case SortField::ShortName:
                    if (titleMetadata != nullptr) item.label = titleMetadata->shortName;
                    break;
                case SortField::LongName:
                    if (titleMetadata != nullptr) item.label = titleMetadata->longName;
                    break;
            }

            // The Wii Menu/vWii record may exist even when MCP cannot return a
            // matching catalog entry. Try the correct OSv0/OSv1 title IDs first;
            // later candidates remain compatibility fallbacks for unusual systems.
            if (slot.kind == ItemKind::Vwii && item.iconPath.empty()) {
                const std::uint64_t wiiMenuId = slot.titleId != 0U
                    ? slot.titleId : 0x0005001010004000ULL;
                char iconPath[112]{};
                std::snprintf(iconPath, sizeof(iconPath),
                    "storage_mlc01:/sys/title/%08X/%08X/meta/iconTex.tga",
                    static_cast<std::uint32_t>(wiiMenuId >> 32U),
                    static_cast<std::uint32_t>(wiiMenuId & 0xFFFFFFFFULL));
                item.iconPath = iconPath;
            } else if (slot.kind == ItemKind::Disc && item.iconPath.empty()) {
                // 00050010-10055000 is the nominal Disc icon ID. The renderer
                // tries its system path before probing the inserted disc volume.
                item.iconPath = "storage_mlc01:/sys/title/00050010/10055000/meta/iconTex.tga";
            }
            if (item.label.empty()) {
                switch (slot.kind) {
                    case ItemKind::Title: item.label = FormatTitleId(slot.titleId); break;
                    case ItemKind::Disc: item.label = "Disc"; break;
                    case ItemKind::Vwii: item.label = "Wii"; break;
                    case ItemKind::Unknown: item.label = "Unknown"; break;
                    case ItemKind::Empty: break;
                    case ItemKind::Folder: break;
                }
            }
        }
    }
    return page;
}

void TitlesSorterSession::BuildPreviewLabels() {
    labels_.fill({});
    for (std::size_t i = 0; i < model_->mainSlots.size(); ++i) {
        const Slot& slot = model_->mainSlots[i];
        if (!slot.occupied) continue;

        switch (slot.kind) {
            case ItemKind::Title: {
                if (metadata_ != nullptr) {
                    const auto found = metadata_->find(slot.titleId);
                    if (found != metadata_->end()) {
                        const TitleMetadata& metadata = found->second;
                        switch (options_.field) {
                            case SortField::TitleId: labels_[i] = FormatTitleId(slot.titleId); break;
                            case SortField::ProductCode: labels_[i] = metadata.productCode; break;
                            case SortField::ShortName: labels_[i] = metadata.shortName; break;
                            case SortField::LongName: labels_[i] = metadata.longName; break;
                        }
                    }
                }
                if (labels_[i].empty()) labels_[i] = FormatTitleId(slot.titleId);
                break;
            }
            case ItemKind::Folder: {
                if (slot.folderId > 0U && slot.folderId <= model_->folders.size())
                    labels_[i] = model_->folders[slot.folderId - 1U].name;
                if (labels_[i].empty()) labels_[i] = "Folder " + std::to_string(slot.folderId);
                break;
            }
            case ItemKind::Disc:
            case ItemKind::Vwii:
            case ItemKind::Unknown: {
                bool resolved = false;
                if (metadata_ != nullptr && slot.titleId != 0U) {
                    const auto found = metadata_->find(slot.titleId);
                    if (found != metadata_->end()) {
                        const TitleMetadata& metadata = found->second;
                        switch (options_.field) {
                            case SortField::TitleId: labels_[i] = FormatTitleId(slot.titleId); break;
                            case SortField::ProductCode: labels_[i] = metadata.productCode; break;
                            case SortField::ShortName: labels_[i] = metadata.shortName; break;
                            case SortField::LongName: labels_[i] = metadata.longName; break;
                        }
                        resolved = !labels_[i].empty();
                    }
                }
                if (!resolved) {
                    if (slot.kind == ItemKind::Disc) labels_[i] = "Disc";
                    else if (slot.kind == ItemKind::Vwii) labels_[i] = "Wii";
                    else labels_[i] = "Unknown";
                }
                break;
            }
            case ItemKind::Empty: break;
        }
    }
}

} // namespace titles_sorter
