#include "core/WiiUMetadataProvider.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace titles_sorter {
namespace {

constexpr std::array<std::string_view, 12> kLanguageTags{
    "ja", "en", "fr", "de", "it", "es", "zhs", "ko", "nl", "pt", "ru", "zht"
};

void AppendUtf8(std::string& out, std::uint32_t cp) {
    if (cp <= 0x7FU) out.push_back(static_cast<char>(cp));
    else if (cp <= 0x7FFU) {
        out.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    } else if (cp <= 0xFFFFU && !(cp >= 0xD800U && cp <= 0xDFFFU)) {
        out.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    } else if (cp <= 0x10FFFFU) {
        out.push_back(static_cast<char>(0xF0U | (cp >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    }
}

bool ParseNumber(std::string_view value, std::uint32_t& result, int base) {
    if (value.empty()) return false;
    result = 0;
    for (char c : value) {
        unsigned digit = 0;
        if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
        else if (base == 16 && c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
        else if (base == 16 && c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
        else return false;
        if (digit >= static_cast<unsigned>(base) || result > (0x10FFFFU - digit) / static_cast<unsigned>(base)) return false;
        result = result * static_cast<unsigned>(base) + digit;
    }
    return result <= 0x10FFFFU && !(result >= 0xD800U && result <= 0xDFFFU);
}

std::string DecodeEntities(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size();) {
        if (value[i] != '&') { out.push_back(value[i++]); continue; }
        const std::size_t end = value.find(';', i + 1);
        if (end == std::string_view::npos || end - i > 12) { out.push_back(value[i++]); continue; }
        const std::string_view entity = value.substr(i + 1, end - i - 1);
        if (entity == "amp") out.push_back('&');
        else if (entity == "apos") out.push_back('\'');
        else if (entity == "quot") out.push_back('"');
        else if (entity == "lt") out.push_back('<');
        else if (entity == "gt") out.push_back('>');
        else if (!entity.empty() && entity.front() == '#') {
            const bool hex = entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X');
            std::uint32_t cp = 0;
            if (ParseNumber(entity.substr(hex ? 2 : 1), cp, hex ? 16 : 10) && cp != 0) AppendUtf8(out, cp);
            else out.append(value.substr(i, end - i + 1));
        } else out.append(value.substr(i, end - i + 1));
        i = end + 1;
    }
    return out;
}

std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
    return value;
}

bool IsXmlWhitespace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool IsNameStart(char c) noexcept {
    const unsigned char value = static_cast<unsigned char>(c);
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           c == '_' || c == ':';
}

bool IsNameChar(char c) noexcept {
    const unsigned char value = static_cast<unsigned char>(c);
    return IsNameStart(c) || (value >= '0' && value <= '9') || c == '-' || c == '.';
}

bool HasBalancedXmlStructure(std::string_view xml) {
    if (xml.size() >= 3U && static_cast<unsigned char>(xml[0]) == 0xEFU &&
        static_cast<unsigned char>(xml[1]) == 0xBBU &&
        static_cast<unsigned char>(xml[2]) == 0xBFU)
        xml.remove_prefix(3U);

    std::vector<std::string_view> stack;
    bool rootSeen = false;
    bool rootClosed = false;
    std::size_t position = 0;

    while (position < xml.size()) {
        if (xml[position] != '<') {
            const std::size_t next = xml.find('<', position);
            const std::size_t end = next == std::string_view::npos ? xml.size() : next;
            if (stack.empty()) {
                for (std::size_t i = position; i < end; ++i)
                    if (!IsXmlWhitespace(xml[i])) return false;
            }
            position = end;
            continue;
        }

        if (xml.substr(position, 4U) == "<!--") {
            const std::size_t end = xml.find("-->", position + 4U);
            if (end == std::string_view::npos) return false;
            position = end + 3U;
            continue;
        }
        if (xml.substr(position, 2U) == "<?") {
            const std::size_t end = xml.find("?>", position + 2U);
            if (end == std::string_view::npos) return false;
            position = end + 2U;
            continue;
        }
        if (xml.substr(position, 9U) == "<![CDATA[") {
            if (stack.empty()) return false;
            const std::size_t end = xml.find("]]>", position + 9U);
            if (end == std::string_view::npos) return false;
            position = end + 3U;
            continue;
        }
        if (xml.substr(position, 2U) == "<!") return false; // DTD/declarations are not needed in Wii U meta.xml.

        const bool closing = xml.substr(position, 2U) == "</";
        std::size_t cursor = position + (closing ? 2U : 1U);
        const std::size_t nameStart = cursor;
        if (cursor >= xml.size() || !IsNameStart(xml[cursor])) return false;
        ++cursor;
        while (cursor < xml.size() && IsNameChar(xml[cursor])) ++cursor;
        const std::string_view name = xml.substr(nameStart, cursor - nameStart);

        if (closing) {
            while (cursor < xml.size() && IsXmlWhitespace(xml[cursor])) ++cursor;
            if (cursor >= xml.size() || xml[cursor] != '>' || stack.empty() || stack.back() != name)
                return false;
            stack.pop_back();
            if (stack.empty()) rootClosed = true;
            position = cursor + 1U;
            continue;
        }

        bool selfClosing = false;
        for (;;) {
            while (cursor < xml.size() && IsXmlWhitespace(xml[cursor])) ++cursor;
            if (cursor >= xml.size()) return false;
            if (xml[cursor] == '>') {
                ++cursor;
                break;
            }
            if (xml[cursor] == '/') {
                if (cursor + 1U >= xml.size() || xml[cursor + 1U] != '>') return false;
                cursor += 2U;
                selfClosing = true;
                break;
            }

            // Validate and skip a quoted attribute. Attribute values are not
            // interpreted because the Wii U fields we consume are element text.
            if (!IsNameStart(xml[cursor])) return false;
            ++cursor;
            while (cursor < xml.size() && IsNameChar(xml[cursor])) ++cursor;
            while (cursor < xml.size() && IsXmlWhitespace(xml[cursor])) ++cursor;
            if (cursor >= xml.size() || xml[cursor] != '=') return false;
            ++cursor;
            while (cursor < xml.size() && IsXmlWhitespace(xml[cursor])) ++cursor;
            if (cursor >= xml.size() || (xml[cursor] != '\'' && xml[cursor] != '"')) return false;
            const char quote = xml[cursor++];
            while (cursor < xml.size() && xml[cursor] != quote) {
                if (xml[cursor] == '<') return false;
                ++cursor;
            }
            if (cursor >= xml.size()) return false;
            ++cursor;
        }

        if (stack.empty()) {
            if (rootSeen || rootClosed) return false;
            rootSeen = true;
        }
        if (!selfClosing) stack.push_back(name);
        else if (stack.empty()) rootClosed = true;
        position = cursor;
    }

    return rootSeen && rootClosed && stack.empty();
}

std::string ReadTag(std::string_view xml, std::string_view tag) {
    std::string open;
    open.reserve(tag.size() + 2);
    open.push_back('<'); open.append(tag); open.push_back('>');
    std::string close;
    close.reserve(tag.size() + 3);
    close.append("</"); close.append(tag); close.push_back('>');
    const std::size_t start = xml.find(open);
    if (start == std::string_view::npos) return {};
    const std::size_t contentStart = start + open.size();
    const std::size_t end = xml.find(close, contentStart);
    if (end == std::string_view::npos) return {};
    return Trim(DecodeEntities(xml.substr(contentStart, end - contentStart)));
}

} // namespace

std::string DecodeXmlEntities(std::string_view value) {
    return DecodeEntities(value);
}

MetaXmlResult ParseWiiUMetaXml(std::string_view xml) {
    MetaXmlResult result;
    if (xml.empty()) { result.error = "meta.xml is empty."; return result; }
    if (!HasBalancedXmlStructure(xml)) { result.error = "meta.xml has an invalid XML structure."; return result; }
    result.metadata.productCode = ReadTag(xml, "product_code");
    for (std::size_t i = 0; i < kLanguageTags.size(); ++i) {
        const std::string suffix(kLanguageTags[i]);
        result.metadata.longNames[i] = ReadTag(xml, "longname_" + suffix);
        result.metadata.shortNames[i] = ReadTag(xml, "shortname_" + suffix);
        result.metadata.publishers[i] = ReadTag(xml, "publisher_" + suffix);
    }
    if (std::all_of(result.metadata.publishers.begin(), result.metadata.publishers.end(),
                    [](const std::string& s) { return s.empty(); })) {
        const std::string genericPublisher = ReadTag(xml, "publisher");
        if (!genericPublisher.empty())
            result.metadata.publishers[static_cast<std::size_t>(SystemLanguage::English)] = genericPublisher;
    }

    if (result.metadata.productCode.empty()) result.warnings.emplace_back("product_code is missing.");
    if (std::all_of(result.metadata.longNames.begin(), result.metadata.longNames.end(),
                    [](const std::string& s) { return s.empty(); }))
        result.warnings.emplace_back("No localized longname fields were found.");
    result.valid = true;
    return result;
}

std::optional<SystemLanguage> ParseSystemLanguage(std::string_view value) noexcept {
    if (value == "0" || value == "ja" || value == "ja-JP") return SystemLanguage::Japanese;
    if (value == "1" || value == "en" || value == "en-US") return SystemLanguage::English;
    if (value == "2" || value == "fr" || value == "fr-FR") return SystemLanguage::French;
    if (value == "3" || value == "de" || value == "de-DE") return SystemLanguage::German;
    if (value == "4" || value == "it" || value == "it-IT") return SystemLanguage::Italian;
    if (value == "5" || value == "es" || value == "es-ES") return SystemLanguage::Spanish;
    if (value == "6" || value == "zh" || value == "zh-CN" || value == "zhs") return SystemLanguage::ChineseSimplified;
    if (value == "7" || value == "ko" || value == "ko-KR") return SystemLanguage::Korean;
    if (value == "8" || value == "nl" || value == "nl-NL") return SystemLanguage::Dutch;
    if (value == "9" || value == "pt" || value == "pt-PT") return SystemLanguage::Portuguese;
    if (value == "10" || value == "ru" || value == "ru-RU") return SystemLanguage::Russian;
    if (value == "11" || value == "zh-TW" || value == "zht") return SystemLanguage::ChineseTraditional;
    return std::nullopt;
}

std::optional<SystemLanguage> ParseSystemLanguageXml(std::string_view xml) {
    if (xml.empty()) return std::nullopt;
    const std::string language = ReadTag(xml, "language");
    if (language.empty()) return std::nullopt;
    return ParseSystemLanguage(language);
}

bool AddTitleMetadata(std::uint64_t titleId,
                      std::string_view metaXml,
                      SystemLanguage language,
                      MetadataCatalog& catalog,
                      std::vector<std::string>& warnings) {
    const MetaXmlResult parsed = ParseWiiUMetaXml(metaXml);
    if (!parsed.valid) {
        warnings.emplace_back("Title " + std::to_string(titleId) + ": " + parsed.error);
        return false;
    }

    for (const std::string& warning : parsed.warnings)
        warnings.emplace_back("Title " + std::to_string(titleId) + ": " + warning);

    catalog.insert_or_assign(titleId, ResolveTitleMetadata(parsed.metadata, language));
    return true;
}

} // namespace titles_sorter
