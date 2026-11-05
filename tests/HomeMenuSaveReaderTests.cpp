#include "core/WiiUHomeMenuSaveReader.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace titles_sorter;

namespace {
class FakeFileSystem final : public IReadOnlySaveFileSystem {
public:
    std::string redirectedPath;
    std::string mlcPath;
    SaveFileStatus redirectedSizeStatus{SaveFileStatus::NotFound};
    SaveFileStatus mlcSizeStatus{SaveFileStatus::Success};
    SaveFileStatus redirectedReadStatus{SaveFileStatus::Success};
    SaveFileStatus mlcReadStatus{SaveFileStatus::Success};
    std::size_t redirectedSize{kExpectedHomeMenuSaveSize};
    std::size_t mlcSize{kExpectedHomeMenuSaveSize};
    int redirectedReads{};
    int mlcReads{};

    SaveFileStatus GetFileSize(const std::string& path, std::size_t& size) override {
        if (path == redirectedPath) {
            size = redirectedSize;
            return redirectedSizeStatus;
        }
        if (path == mlcPath) {
            size = mlcSize;
            return mlcSizeStatus;
        }
        return SaveFileStatus::NotFound;
    }

    SaveFileStatus ReadExact(const std::string& path, std::uint8_t* destination, std::size_t size) override {
        if (path == redirectedPath) {
            ++redirectedReads;
            if (redirectedReadStatus == SaveFileStatus::Success) std::memset(destination, 0xA5, size);
            return redirectedReadStatus;
        }
        if (path == mlcPath) {
            ++mlcReads;
            if (mlcReadStatus == SaveFileStatus::Success) std::memset(destination, 0x5A, size);
            return mlcReadStatus;
        }
        return SaveFileStatus::NotFound;
    }
};

WiiUHomeMenuSaveIdentity MakeIdentity() {
    return {0x0005001010040000ULL, 0x12345678U, "WUP12345678901", false};
}

void TestUsesMlcWhenRedirectDoesNotExist() {
    auto identity = MakeIdentity();
    FakeFileSystem fs;
    std::string error;
    auto redirectedIdentity = identity;
    redirectedIdentity.pluginRedirectedSaveExists = true;
    auto redirectedLocation = ResolveHomeMenuSaveLocation(redirectedIdentity, error);
    auto mlcLocation = ResolveHomeMenuSaveLocation(identity, error);
    assert(redirectedLocation && mlcLocation);
    fs.redirectedPath = *redirectedLocation->pluginRedirectedPath;
    fs.mlcPath = mlcLocation->mlcPath;
    WiiUHomeMenuSaveSnapshot snapshot;
    assert(ReadActiveHomeMenuSave(identity, fs, snapshot, error));
    assert(!snapshot.location.usesPluginRedirect);
    assert(fs.redirectedReads == 0 && fs.mlcReads == 1);
    assert(snapshot.bytes.size() == kExpectedHomeMenuSaveSize);
    assert(snapshot.bytes.front() == 0x5A);
}

void TestPluginSaveTakesPrecedence() {
    auto identity = MakeIdentity();
    FakeFileSystem fs;
    std::string error;
    auto redirectedIdentity = identity;
    redirectedIdentity.pluginRedirectedSaveExists = true;
    auto redirectedLocation = ResolveHomeMenuSaveLocation(redirectedIdentity, error);
    auto mlcLocation = ResolveHomeMenuSaveLocation(identity, error);
    assert(redirectedLocation && mlcLocation);
    fs.redirectedPath = *redirectedLocation->pluginRedirectedPath;
    fs.mlcPath = mlcLocation->mlcPath;
    fs.redirectedSizeStatus = SaveFileStatus::Success;
    WiiUHomeMenuSaveSnapshot snapshot;
    assert(ReadActiveHomeMenuSave(identity, fs, snapshot, error));
    assert(snapshot.location.usesPluginRedirect);
    assert(fs.redirectedReads == 1 && fs.mlcReads == 0);
    assert(snapshot.bytes.front() == 0xA5);
}

void TestInvalidPluginSaveNeverFallsBackToMlc() {
    auto identity = MakeIdentity();
    FakeFileSystem fs;
    std::string error;
    auto redirectedIdentity = identity;
    redirectedIdentity.pluginRedirectedSaveExists = true;
    auto redirectedLocation = ResolveHomeMenuSaveLocation(redirectedIdentity, error);
    auto mlcLocation = ResolveHomeMenuSaveLocation(identity, error);
    assert(redirectedLocation && mlcLocation);
    fs.redirectedPath = *redirectedLocation->pluginRedirectedPath;
    fs.mlcPath = mlcLocation->mlcPath;
    fs.redirectedSizeStatus = SaveFileStatus::Success;
    fs.redirectedSize = kExpectedHomeMenuSaveSize - 1U;
    WiiUHomeMenuSaveSnapshot snapshot;
    assert(!ReadActiveHomeMenuSave(identity, fs, snapshot, error));
    assert(fs.redirectedReads == 0 && fs.mlcReads == 0);
    assert(error.find("Unsupported Home Menu save size") != std::string::npos);
}

void TestPluginReadFailureNeverFallsBack() {
    auto identity = MakeIdentity();
    FakeFileSystem fs;
    std::string error;
    auto redirectedIdentity = identity;
    redirectedIdentity.pluginRedirectedSaveExists = true;
    auto redirectedLocation = ResolveHomeMenuSaveLocation(redirectedIdentity, error);
    auto mlcLocation = ResolveHomeMenuSaveLocation(identity, error);
    assert(redirectedLocation && mlcLocation);
    fs.redirectedPath = *redirectedLocation->pluginRedirectedPath;
    fs.mlcPath = mlcLocation->mlcPath;
    fs.redirectedSizeStatus = SaveFileStatus::Success;
    fs.redirectedReadStatus = SaveFileStatus::IoError;
    WiiUHomeMenuSaveSnapshot snapshot;
    assert(!ReadActiveHomeMenuSave(identity, fs, snapshot, error));
    assert(fs.redirectedReads == 1 && fs.mlcReads == 0);
}

void TestCannotAssumeNoRedirectWithoutSerial() {
    auto identity = MakeIdentity();
    identity.consoleSerialId.clear();
    FakeFileSystem fs;
    WiiUHomeMenuSaveSnapshot snapshot;
    std::string error;
    assert(!ReadActiveHomeMenuSave(identity, fs, snapshot, error));
    assert(error.find("serial identity is unavailable") != std::string::npos);
}

void TestRedirectProbeIoErrorFailsClosed() {
    auto identity = MakeIdentity();
    FakeFileSystem fs;
    std::string error;
    auto redirectedIdentity = identity;
    redirectedIdentity.pluginRedirectedSaveExists = true;
    auto redirectedLocation = ResolveHomeMenuSaveLocation(redirectedIdentity, error);
    assert(redirectedLocation);
    fs.redirectedPath = *redirectedLocation->pluginRedirectedPath;
    fs.redirectedSizeStatus = SaveFileStatus::IoError;
    WiiUHomeMenuSaveSnapshot snapshot;
    assert(!ReadActiveHomeMenuSave(identity, fs, snapshot, error));
    assert(fs.mlcReads == 0);
}
} // namespace

int main() {
    TestUsesMlcWhenRedirectDoesNotExist();
    TestPluginSaveTakesPrecedence();
    TestInvalidPluginSaveNeverFallsBackToMlc();
    TestPluginReadFailureNeverFallsBack();
    TestCannotAssumeNoRedirectWithoutSerial();
    TestRedirectProbeIoErrorFailsClosed();
    std::cout << "All Home Menu save reader tests passed.\n";
}
