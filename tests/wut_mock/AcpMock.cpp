#include "AcpMock.hpp"

#include <cstring>
#include <unordered_map>

namespace {
std::unordered_map<std::uint64_t, ACPMetaXml> gMetadata;
}

void SetMockAcpMetadata(std::uint64_t titleId, const ACPMetaXml& metadata) {
    gMetadata.insert_or_assign(titleId, metadata);
}

void ResetMockAcpMetadata() {
    gMetadata.clear();
}

extern "C" std::int32_t ACPGetTitleMetaXml(std::uint64_t titleId, ACPMetaXml* outMetadata) {
    if (outMetadata == nullptr) return -1;
    const auto found = gMetadata.find(titleId);
    if (found == gMetadata.end()) return -1;
    std::memcpy(outMetadata, &found->second, sizeof(ACPMetaXml));
    return 0;
}
