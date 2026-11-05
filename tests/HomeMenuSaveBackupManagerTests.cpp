#include "core/HomeMenuSaveBackupManager.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    using namespace titles_sorter;
    const std::string root = "backup_manager_test";
    const std::string source = "tests/fixtures/sample_menu_save.dat";
    const std::string active = root + "/active.dat";
    const std::string backups = root + "/backups";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    assert(!filesystemError);
    std::filesystem::create_directories(backups, filesystemError);
    assert(!filesystemError);
    {
        std::ifstream in(source, std::ios::binary);
        std::ofstream out(active, std::ios::binary);
        out << in.rdbuf();
        assert(in.good() || in.eof());
        assert(out.good());
    }

    HomeMenuSaveBackupManager manager;
    std::string created;
    std::string error;
    assert(manager.CreateBackup(active, backups, SaveBackupKind::Manual, 10U, created, error));
    assert(created.find("/backup_") != std::string::npos);

    std::vector<SaveBackupEntry> entries;
    assert(manager.ListBackups(backups, entries, error));
    assert(entries.size() == 1U && !entries[0].automatic);

    // Manual and automatic backups must share one chronological ordering.
    const std::string sortedBackups = root + "/sorted_backups";
    std::filesystem::create_directories(sortedBackups, filesystemError);
    assert(!filesystemError);
    const std::vector<std::string> mixedBackupNames = {
        "backup_2024-02-01_10-00-00.dat",
        "autobackup_2025-06-15_08-30-00.dat",
        "backup_2023-12-31_23-59-59.dat",
        "autobackup_2024-11-20_12-00-00.dat"
    };
    for (const std::string& name : mixedBackupNames) {
        std::ofstream out(sortedBackups + "/" + name, std::ios::binary);
        out << "test";
        assert(out.good());
    }
    assert(manager.ListBackups(sortedBackups, entries, error));
    assert(entries.size() == 4U);
    assert(entries[0].fileName == "autobackup_2025-06-15_08-30-00.dat");
    assert(entries[1].fileName == "autobackup_2024-11-20_12-00-00.dat");
    assert(entries[2].fileName == "backup_2024-02-01_10-00-00.dat");
    assert(entries[3].fileName == "backup_2023-12-31_23-59-59.dat");

    // Retention applies only to autobackups; manual backups are never pruned.
    const std::string oldAuto1 = backups + "/autobackup_2020-01-01_00-00-00.dat";
    const std::string oldAuto2 = backups + "/autobackup_2021-01-01_00-00-00.dat";
    for (const std::string& oldPath : {oldAuto1, oldAuto2}) {
        std::ifstream in(source, std::ios::binary);
        std::ofstream out(oldPath, std::ios::binary);
        out << in.rdbuf();
        assert(out.good());
    }
    std::string autoCreated;
    assert(manager.CreateBackup(active, backups, SaveBackupKind::Automatic, 1U,
                                autoCreated, error));
    assert(!autoCreated.empty());
    assert(std::ifstream(oldAuto1).fail());
    assert(std::ifstream(oldAuto2).fail());
    assert(std::ifstream(created).good());

    // Corrupt the active file; restore must validate the backup and replace it.
    {
        std::ofstream out(active, std::ios::binary | std::ios::trunc);
        out << "broken";
    }
    std::string restoreAutobackup;
    assert(manager.RestoreBackup(created, active, backups, false, 10U, restoreAutobackup, error));
    std::ifstream restored(active, std::ios::binary);
    std::ifstream original(source, std::ios::binary);
    const std::string restoredBytes((std::istreambuf_iterator<char>(restored)), {});
    const std::string originalBytes((std::istreambuf_iterator<char>(original)), {});
    restored.close();
    original.close();
    assert(restoredBytes == originalBytes);

    // When enabled, Restore first preserves the current active save as an
    // automatic backup and only then installs the selected backup.
    {
        std::ifstream in(source, std::ios::binary);
        std::ofstream out(active, std::ios::binary | std::ios::trunc);
        out << in.rdbuf();
        assert(out.good());
    }
    // Change one byte while preserving the valid save size.
    {
        std::fstream file(active, std::ios::binary | std::ios::in | std::ios::out);
        char value = 'X';
        file.seekp(0);
        file.write(&value, 1);
        assert(file.good());
    }
    std::string expectedPreRestore;
    {
        std::ifstream in(active, std::ios::binary);
        expectedPreRestore.assign(std::istreambuf_iterator<char>(in), {});
    }
    std::string restoreAutomatic;
    assert(manager.RestoreBackup(created, active, backups, true, 10U,
                                 restoreAutomatic, error));
    assert(!restoreAutomatic.empty());
    assert(restoreAutomatic.find("autobackup_") != std::string::npos);
    std::ifstream preserved(restoreAutomatic, std::ios::binary);
    const std::string preservedBytes((std::istreambuf_iterator<char>(preserved)), {});
    assert(preservedBytes == expectedPreRestore);
    preserved.close();

    // A selected automatic backup may be pruned by retention. Restore stages
    // and verifies it first, so the operation remains valid after pruning.
    const std::string selectedAutoDirectory = root + "/selected_auto_backups";
    std::filesystem::create_directories(selectedAutoDirectory, filesystemError);
    assert(!filesystemError);
    const std::string selectedAuto = selectedAutoDirectory +
        "/autobackup_2020-01-01_00-00-00.dat";
    {
        std::ifstream in(source, std::ios::binary);
        std::ofstream out(selectedAuto, std::ios::binary);
        out << in.rdbuf();
        assert(out.good());
    }
    const std::string retentionActive = root + "/retention_active.dat";
    {
        std::ifstream in(source, std::ios::binary);
        std::ofstream out(retentionActive, std::ios::binary);
        out << in.rdbuf();
        assert(out.good());
    }
    {
        std::fstream file(retentionActive, std::ios::binary | std::ios::in | std::ios::out);
        const char value = 'Y';
        file.seekp(0);
        file.write(&value, 1);
        assert(file.good());
    }
    std::string retentionAutobackup;
    assert(manager.RestoreBackup(selectedAuto, retentionActive, selectedAutoDirectory,
                                 true, 1U, retentionAutobackup, error));
    assert(!retentionAutobackup.empty());
    assert(std::ifstream(selectedAuto).fail()); // Retention removed the original entry.
    {
        std::ifstream restoredSelected(retentionActive, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(restoredSelected)), {});
        assert(bytes == originalBytes);
    }

    // If the requested autobackup cannot be created, Restore must abort and
    // remove its staged candidate without changing the active save.
    const std::string failedRestoreActive = root + "/failed_restore_active.dat";
    {
        std::ifstream in(source, std::ios::binary);
        std::ofstream out(failedRestoreActive, std::ios::binary);
        out << in.rdbuf();
        assert(out.good());
    }
    std::string beforeFailedRestore;
    {
        std::ifstream in(failedRestoreActive, std::ios::binary);
        beforeFailedRestore.assign(std::istreambuf_iterator<char>(in), {});
    }
    std::string failedRestoreAutobackup;
    assert(!manager.RestoreBackup(created, failedRestoreActive, backups, true, 0U,
                                  failedRestoreAutobackup, error));
    assert(failedRestoreAutobackup.empty());
    assert(std::ifstream(failedRestoreActive + ".restore.tmp").fail());
    {
        std::ifstream in(failedRestoreActive, std::ios::binary);
        const std::string afterFailedRestore((std::istreambuf_iterator<char>(in)), {});
        assert(afterFailedRestore == beforeFailedRestore);
    }

    // Startup recovery rolls back a Restore interrupted after the previous
    // active save was renamed aside but before candidate verification.
    const std::string previous = active + ".restore.previous";
    {
        std::ifstream in(source, std::ios::binary);
        std::ofstream out(previous, std::ios::binary);
        out << in.rdbuf();
        assert(out.good());
    }
    {
        std::ofstream out(active, std::ios::binary | std::ios::trunc);
        out << "interrupted candidate";
        assert(out.good());
    }
    if (!manager.RecoverInterruptedRestore(active, error)) {
        std::fprintf(stderr, "RecoverInterruptedRestore failed: %s\n", error.c_str());
        assert(false);
    }
    std::ifstream recovered(active, std::ios::binary);
    const std::string recoveredBytes((std::istreambuf_iterator<char>(recovered)), {});
    recovered.close();
    assert(recoveredBytes == originalBytes);
    assert(std::ifstream(previous).fail());

    std::remove(active.c_str());
    std::remove(created.c_str());
    std::remove(autoCreated.c_str());
    std::remove(restoreAutomatic.c_str());
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    if (cleanupError) {
        std::fprintf(stderr, "Unable to clean backup-manager test directory: %s\n",
                     cleanupError.message().c_str());
        assert(false);
    }
    return 0;
}
