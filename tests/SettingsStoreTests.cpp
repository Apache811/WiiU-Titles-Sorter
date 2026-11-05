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

    options.field = SortField::ProductCode;
    options.folderPlacement = FolderPlacement::MixedAlphabetically;
    options.sortFolderContents = false;
    options.excludedPages = {1U, 3U, 8U};
    assert(store.SaveIfChanged(path, options, error));

    SortOptions loaded;
    TitlesSorterSettingsStore secondStore;
    assert(secondStore.Load(path, loaded, error));
    assert(loaded.field == options.field);
    assert(loaded.folderPlacement == options.folderPlacement);
    assert(loaded.sortFolderContents == options.sortFolderContents);
    assert(loaded.excludedPages == options.excludedPages);
    assert(secondStore.SaveIfChanged(path, loaded, error)); // No-op when unchanged.

    // Recover a completed settings file if power is lost after the old file
    // has been moved to the backup but before the temporary file is installed.
    assert(std::rename(path, "titles_sorter_settings_test.ini.bak") == 0);
    TitlesSorterSettingsStore recoveryStore;
    SortOptions recovered;
    assert(recoveryStore.Load(path, recovered, error));
    assert(recovered.field == options.field);
    assert(recovered.excludedPages == options.excludedPages);

    std::remove(path);
    std::remove("titles_sorter_settings_test.ini.tmp");
    std::remove("titles_sorter_settings_test.ini.bak");
    return 0;
}
