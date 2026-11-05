#include "platform/wiiu/WiiUTitlesSorterCoordinator.hpp"

#include <algorithm>
#include <unordered_set>

namespace titles_sorter {
namespace {
constexpr std::uint64_t kHomebrewTitleHigh = 0x0005000FULL;

void AddHomebrewId(const Slot& slot, std::unordered_set<std::uint64_t>& unique,
                   std::vector<std::uint64_t>& output) {
    if (!slot.occupied || slot.kind != ItemKind::Title ||
        (slot.titleId >> 32U) != kHomebrewTitleHigh) return;
    if (unique.insert(slot.titleId).second) output.push_back(slot.titleId);
}
} // namespace

bool WiiUTitlesSorterCoordinator::Begin(std::span<const std::uint8_t> saveBytes,
                                        std::string& error) {
    Reset();
    if (!session_.LoadSave(saveBytes, error)) {
        state_ = CoordinatorState::Failed;
        fatalError_ = error;
        return false;
    }
    // Request metadata only for IDs referenced by the Home Menu save. Unlike
    // the legacy game-family filter, this also covers installed system apps
    // (for example Mii Maker) without parsing metadata for every installed title.
    std::vector<std::uint64_t> requestedMetadataIds;
    std::unordered_set<std::uint64_t> uniqueMetadataIds;
    const SaveModel* model = session_.Model();
    if (model != nullptr) {
        uniqueMetadataIds.reserve(32U);
        const auto collect = [&uniqueMetadataIds, &requestedMetadataIds](const Slot& slot) {
            if (!slot.occupied || slot.kind == ItemKind::Empty ||
                slot.kind == ItemKind::Folder || slot.titleId == 0U) return;
            if (uniqueMetadataIds.insert(slot.titleId).second)
                requestedMetadataIds.push_back(slot.titleId);
        };
        for (const Slot& slot : model->mainSlots) collect(slot);
        for (const Folder& folder : model->folders) {
            if (!folder.active) continue;
            for (const Slot& slot : folder.slots) collect(slot);
        }
    }

    if (!metadataProvider_.Begin(requestedMetadataIds, error)) {
        state_ = CoordinatorState::Failed;
        fatalError_ = error;
        return false;
    }
    state_ = metadataProvider_.IsComplete()
        ? CoordinatorState::LoadingHomebrewMetadata
        : CoordinatorState::LoadingNativeMetadata;
    if (state_ == CoordinatorState::LoadingHomebrewMetadata) CollectHomebrewIds();
    if (homebrewIds_.empty() && state_ == CoordinatorState::LoadingHomebrewMetadata) {
        const auto& providerWarnings = metadataProvider_.Warnings();
        warnings_.insert(warnings_.end(), providerWarnings.begin(), providerWarnings.end());
        session_.SetMetadata(&metadataProvider_.Catalog());
        state_ = CoordinatorState::Ready;
    }
    return true;
}

void WiiUTitlesSorterCoordinator::Update(std::string& error) {
    error.clear();
    if (state_ == CoordinatorState::Failed) {
        error = fatalError_;
        return;
    }

    if (state_ == CoordinatorState::LoadingNativeMetadata) {
        if (!metadataProvider_.LoadStep(error)) {
            state_ = CoordinatorState::Failed;
            fatalError_ = error;
            return;
        }
        if (metadataProvider_.IsComplete()) {
            CollectHomebrewIds();
            state_ = CoordinatorState::LoadingHomebrewMetadata;
            if (homebrewIds_.empty()) {
                session_.SetMetadata(&metadataProvider_.Catalog());
                state_ = CoordinatorState::Ready;
            }
        }
        return;
    }

    if (state_ == CoordinatorState::LoadingHomebrewMetadata) {
        if (nextHomebrewIndex_ < homebrewIds_.size()) {
            std::string warning;
            (void)metadataProvider_.AddHomebrewTitleMetadata(homebrewIds_[nextHomebrewIndex_], warning);
            ++nextHomebrewIndex_;
        }
        if (nextHomebrewIndex_ >= homebrewIds_.size()) {
            const auto& providerWarnings = metadataProvider_.Warnings();
            warnings_.insert(warnings_.end(), providerWarnings.begin(), providerWarnings.end());
            session_.SetMetadata(&metadataProvider_.Catalog());
            state_ = CoordinatorState::Ready;
        }
    }
}

void WiiUTitlesSorterCoordinator::Reset() {
    state_ = CoordinatorState::Idle;
    session_ = TitlesSorterSession{};
    metadataProvider_.Reset();
    homebrewIds_.clear();
    nextHomebrewIndex_ = 0;
    warnings_.clear();
    fatalError_.clear();
}

CoordinatorState WiiUTitlesSorterCoordinator::State() const noexcept { return state_; }
bool WiiUTitlesSorterCoordinator::IsLoading() const noexcept {
    return state_ == CoordinatorState::LoadingNativeMetadata ||
           state_ == CoordinatorState::LoadingHomebrewMetadata;
}
bool WiiUTitlesSorterCoordinator::IsReady() const noexcept { return state_ == CoordinatorState::Ready; }
const TitlesSorterSession& WiiUTitlesSorterCoordinator::Session() const noexcept { return session_; }
TitlesSorterSession& WiiUTitlesSorterCoordinator::Session() noexcept { return session_; }
const std::vector<std::string>& WiiUTitlesSorterCoordinator::Warnings() const noexcept { return warnings_; }
std::size_t WiiUTitlesSorterCoordinator::HomebrewMetadataProcessed() const noexcept { return nextHomebrewIndex_; }
std::size_t WiiUTitlesSorterCoordinator::HomebrewMetadataTotal() const noexcept { return homebrewIds_.size(); }

void WiiUTitlesSorterCoordinator::CollectHomebrewIds() {
    homebrewIds_.clear();
    nextHomebrewIndex_ = 0;
    const SaveModel* model = session_.Model();
    if (model == nullptr) return;

    std::unordered_set<std::uint64_t> unique;
    unique.reserve(16U);
    for (const Slot& slot : model->mainSlots) AddHomebrewId(slot, unique, homebrewIds_);
    for (const Folder& folder : model->folders) {
        if (!folder.active) continue;
        for (const Slot& slot : folder.slots) AddHomebrewId(slot, unique, homebrewIds_);
    }
}

} // namespace titles_sorter
