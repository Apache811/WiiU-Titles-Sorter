#include "core/TitlesSorterCore.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace titles_sorter;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: titles_sorter_dryrun <save-file> [excluded-pages]\n";
        return 2;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Unable to open input save file.\n";
        return 2;
    }

    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()
    };

    const ParseResult parsed = ParseSave(bytes);
    if (!parsed) {
        for (const std::string& error : parsed.errors) {
            std::cerr << "Parse error: " << error << '\n';
        }
        return 1;
    }

    SortOptions options;
    options.field = SortField::TitleId;
    options.folderPlacement = FolderPlacement::FoldersFirst;

    if (argc >= 3) {
        std::string error;
        auto pages = ParseExcludedPages(argv[2], 20, error);
        if (!pages) {
            std::cerr << "Invalid excluded pages: " << error << '\n';
            return 2;
        }
        options.excludedPages = std::move(*pages);
    }

    const MetadataCatalog emptyCatalog;
    const PlanResult plan = BuildDryRunPlan(*parsed.model, emptyCatalog, options);
    if (!plan.valid) {
        for (const std::string& error : plan.errors) {
            std::cerr << "Planning error: " << error << '\n';
        }
        return 1;
    }

    std::size_t activeFolders = 0;
    for (const Folder& folder : parsed.model->folders) {
        if (folder.active) ++activeFolders;
    }

    std::cout << "Titles Sorter — DRY RUN ONLY\n";
    std::cout << "Input bytes: " << bytes.size() << '\n';
    std::cout << "Main slots: " << kMainSlotCount << '\n';
    std::cout << "Active folders: " << activeFolders << '\n';
    std::cout << "Planned moves: " << plan.moves.size() << '\n';
    std::cout << "Save file modified: NO\n\n";

    for (const Move& move : plan.moves) {
        std::cout << (move.folderId ? "Folder " + std::to_string(*move.folderId) : "Home Menu")
                  << ": " << move.sourceIndex << " -> " << move.destinationIndex
                  << " (" << ToString(move.kind) << ", ID 0x"
                  << std::hex << move.itemId << std::dec << ")\n";
    }

    for (const std::string& warning : plan.warnings) {
        std::cout << "Warning: " << warning << '\n';
    }
    return 0;
}
