#include "core/HomeMenuSaveApplyTransaction.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace titles_sorter;
namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> ReadBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    assert(input.good());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    assert(output.good());
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    assert(output.good());
}

void TestApplyWithAutomaticBackup() {
    const fs::path root = fs::temp_directory_path() / "titles_sorter_apply_test";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root);
    const fs::path active = root / "active.dat";
    const fs::path backups = root / "backups";
    const auto original = ReadBytes("tests/fixtures/sample_menu_save.dat");
    WriteBytes(active, original);

    ParseResult parsed = ParseSave(original);
    assert(parsed);
    MetadataCatalog metadata;
    PlanResult plan = BuildDryRunPlan(*parsed.model, metadata, SortOptions{});
    assert(plan.valid);

    HomeMenuSaveApplyTransaction transaction;
    ApplyTransactionOptions options;
    options.createAutomaticBackup = true;
    options.maxAutomaticBackups = 10U;
    const ApplyTransactionResult result = transaction.Apply(
        *parsed.model, plan, active.string(), backups.string(), options);
    assert(result.applied);
    assert(result.backupCreated);
    assert(fs::exists(result.backupPath));
    assert(ReadBytes(active) == original || ParseSave(ReadBytes(active)));
    assert(!fs::exists(active.string() + ".apply.previous"));
    assert(!fs::exists(active.string() + ".apply.tmp"));
    fs::remove_all(root, ignored);
}

void TestRejectsStaleActiveSaveWithoutBackupOrMutation() {
    const fs::path root = fs::temp_directory_path() / "titles_sorter_apply_stale_test";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root);
    const fs::path active = root / "active.dat";
    auto original = ReadBytes("tests/fixtures/sample_menu_save.dat");
    ParseResult parsed = ParseSave(original);
    assert(parsed);
    MetadataCatalog metadata;
    PlanResult plan = BuildDryRunPlan(*parsed.model, metadata, SortOptions{});
    assert(plan.valid);

    original[0x2700U] ^= 0x01U;
    WriteBytes(active, original);
    HomeMenuSaveApplyTransaction transaction;
    ApplyTransactionOptions options;
    options.createAutomaticBackup = true;
    const ApplyTransactionResult result = transaction.Apply(
        *parsed.model, plan, active.string(), (root / "backups").string(), options);
    assert(!result.applied);
    assert(!result.backupCreated);
    assert(ReadBytes(active) == original);
    assert(!fs::exists(root / "backups"));
    fs::remove_all(root, ignored);
}

void TestRecoversInterruptedApplyByRestoringOriginal() {
    const fs::path root = fs::temp_directory_path() / "titles_sorter_apply_recovery_test";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root);
    const fs::path active = root / "active.dat";
    const auto original = ReadBytes("tests/fixtures/sample_menu_save.dat");
    auto interruptedCandidate = original;
    interruptedCandidate[0x2700U] ^= 0x01U;
    WriteBytes(active.string() + ".apply.previous", original);
    WriteBytes(active, interruptedCandidate);
    WriteBytes(active.string() + ".apply.tmp", interruptedCandidate);

    HomeMenuSaveApplyTransaction transaction;
    std::string error;
    assert(transaction.RecoverInterruptedApply(active.string(), error));
    assert(ReadBytes(active) == original);
    assert(!fs::exists(active.string() + ".apply.previous"));
    assert(!fs::exists(active.string() + ".apply.tmp"));
    assert(!fs::exists(active.string() + ".apply.failed"));
    fs::remove_all(root, ignored);
}

void TestApplyWithoutAutomaticBackup() {
    const fs::path root = fs::temp_directory_path() / "titles_sorter_apply_no_backup_test";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root);
    const fs::path active = root / "active.dat";
    const auto original = ReadBytes("tests/fixtures/sample_menu_save.dat");
    WriteBytes(active, original);
    ParseResult parsed = ParseSave(original);
    assert(parsed);
    MetadataCatalog metadata;
    PlanResult plan = BuildDryRunPlan(*parsed.model, metadata, SortOptions{});
    assert(plan.valid);

    HomeMenuSaveApplyTransaction transaction;
    ApplyTransactionOptions options;
    options.createAutomaticBackup = false;
    const ApplyTransactionResult result = transaction.Apply(
        *parsed.model, plan, active.string(), (root / "backups").string(), options);
    assert(result.applied);
    assert(!result.backupCreated);
    assert(!fs::exists(root / "backups"));
    assert(ParseSave(ReadBytes(active)));
    fs::remove_all(root, ignored);
}

} // namespace

int main() {
    TestApplyWithAutomaticBackup();
    TestRejectsStaleActiveSaveWithoutBackupOrMutation();
    TestApplyWithoutAutomaticBackup();
    TestRecoversInterruptedApplyByRestoringOriginal();
    std::puts("HomeMenuSaveApplyTransactionTests passed.");
    return 0;
}
