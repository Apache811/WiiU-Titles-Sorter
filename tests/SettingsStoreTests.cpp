#include "core/TitlesSorterSettingsStore.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <string>

int main() {
    using namespace titles_sorter;
    constexpr const char* path = "titles_sorter_settings_test.ini";
    std::remove(path);
    std::remove("titles_sorter_settings_test.ini.tmp");

    TitlesSorterSettingsStore store;
    SortOptions options;
    std::string error;
    assert(store.Load(path, options, error)); // Missing file means defaults.
    assert(options.field == SortField::LongName);
    assert(options.folderPlacement == FolderPlacement::FoldersFirst);
    assert(options.sortFolderContents);
    assert(options.autoBackup);
    assert(options.maxAutoBackups == 10U);

    options.field = SortField::ProductCode;
    options.folderPlacement = FolderPlacement::MixedAlphabetically;
    options.sortFolderContents = false;
    options.excludedPages = {1U, 3U, 8U};
    options.autoBackup = false;
    options.maxAutoBackups = 7U;
    assert(store.SaveIfChanged(path, options, error));

    SortOptions loaded;
    TitlesSorterSettingsStore secondStore;
    assert(secondStore.Load(path, loaded, error));
    assert(loaded.field == options.field);
    assert(loaded.folderPlacement == options.folderPlacement);
    assert(loaded.sortFolderContents == options.sortFolderContents);
    assert(loaded.excludedPages == options.excludedPages);
    assert(loaded.autoBackup == options.autoBackup);
    assert(loaded.maxAutoBackups == options.maxAutoBackups);
    assert(secondStore.SaveIfChanged(path, loaded, error)); // No-op when unchanged.

    // Recover a completed settings file if power is lost after the old file
    // has been moved to the backup but before the temporary file is installed.
    assert(std::rename(path, "titles_sorter_settings_test.ini.bak") == 0);
    TitlesSorterSettingsStore recoveryStore;
    SortOptions recovered;
    assert(recoveryStore.Load(path, recovered, error));
    assert(recovered.field == options.field);
    assert(recovered.excludedPages == options.excludedPages);

    // Version 1 settings remain readable and receive v2 backup defaults.
    {
        std::FILE* legacy = std::fopen(path, "wb");
        assert(legacy != nullptr);
        const char legacyContent[] =
            "version=1\n"
            "sort_field=2\n"
            "folder_placement=1\n"
            "sort_folder_contents=1\n"
            "sort_folders_by_contents=0\n"
            "excluded_pages=\n";
        assert(std::fwrite(legacyContent, 1U, sizeof(legacyContent) - 1U, legacy) ==
               sizeof(legacyContent) - 1U);
        assert(std::fclose(legacy) == 0);
    }
    TitlesSorterSettingsStore legacyStore;
    SortOptions legacyOptions;
    assert(legacyStore.Load(path, legacyOptions, error));
    assert(legacyOptions.autoBackup);
    assert(legacyOptions.maxAutoBackups == 10U);

    std::remove(path);
    std::remove("titles_sorter_settings_test.ini.tmp");
    std::remove("titles_sorter_settings_test.ini.bak");
    return 0;
}
