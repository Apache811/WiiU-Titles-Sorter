#pragma once

#include "core/TitlesSorterCore.hpp"

#include <optional>
#include <string_view>

namespace titles_sorter {

// Metadata read exclusively from a title's own meta/meta.xml.
// This parser is platform-independent so it can be tested on desktop builds.
struct MetaXmlResult {
    bool valid{};
    LocalizedTitleMetadata metadata;
    std::vector<std::string> warnings;
    std::string error;
};

MetaXmlResult ParseWiiUMetaXml(std::string_view xml);

// Decode XML entities in metadata values obtained from XML, ACP or Homebrew on Menu INI.
std::string DecodeXmlEntities(std::string_view value);
std::optional<SystemLanguage> ParseSystemLanguage(std::string_view value) noexcept;
std::optional<SystemLanguage> ParseSystemLanguageXml(std::string_view xml);

// Adds one installed title's metadata to the in-memory catalog. No external
// source is consulted. Invalid XML is reported to the caller and not inserted.
bool AddTitleMetadata(std::uint64_t titleId,
                      std::string_view metaXml,
                      SystemLanguage language,
                      MetadataCatalog& catalog,
                      std::vector<std::string>& warnings);

} // namespace titles_sorter
