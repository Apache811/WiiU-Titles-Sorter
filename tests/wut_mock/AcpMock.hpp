#pragma once

#include <nn/acp.h>

#include <cstdint>
#include <unordered_map>

void SetMockAcpMetadata(std::uint64_t titleId, const ACPMetaXml& metadata);
void ResetMockAcpMetadata();
