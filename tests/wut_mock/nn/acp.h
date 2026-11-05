#pragma once

#include <cstdint>

// Host-only subset of WUT ACPMetaXml used by the metadata adapter tests.
// This intentionally models field names, not the full ACP ABI layout.
typedef struct ACPMetaXml {
    char longname_ja[128];
    char longname_en[128];
    char longname_fr[128];
    char longname_de[128];
    char longname_it[128];
    char longname_es[128];
    char longname_zhs[128];
    char longname_ko[128];
    char longname_nl[128];
    char longname_pt[128];
    char longname_ru[128];
    char longname_zht[128];
    char shortname_ja[64];
    char shortname_en[64];
    char shortname_fr[64];
    char shortname_de[64];
    char shortname_it[64];
    char shortname_es[64];
    char shortname_zhs[64];
    char shortname_ko[64];
    char shortname_nl[64];
    char shortname_pt[64];
    char shortname_ru[64];
    char shortname_zht[64];
    char publisher_ja[128];
    char publisher_en[128];
    char publisher_fr[128];
    char publisher_de[128];
    char publisher_it[128];
    char publisher_es[128];
    char publisher_zhs[128];
    char publisher_ko[128];
    char publisher_nl[128];
    char publisher_pt[128];
    char publisher_ru[128];
    char publisher_zht[128];
    char product_code[16];
} ACPMetaXml;

#ifdef __cplusplus
extern "C" {
#endif
int32_t ACPGetTitleMetaXml(uint64_t titleId, ACPMetaXml* outMetadata);
#ifdef __cplusplus
}
#endif
