#include "core/WiiUHomeMenuSaveLocator.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

using namespace titles_sorter;

int main() {
    std::string error;
    WiiUHomeMenuSaveIdentity identity;
    identity.systemMenuTitleId = 0x0005001010040100ULL;
    identity.userPersistentId = 0x1234ABCDU;
    identity.consoleSerialId = "FW123456789";

    auto location = ResolveHomeMenuSaveLocation(identity, error);
    assert(location.has_value());
    assert(error.empty());
    assert(location->activePath ==
        "storage_mlc:/usr/save/00050010/10040100/user/9234abcd/BaristaAccountSaveFile.dat");
    assert(location->mlcPath == location->activePath);
    assert(!location->usesPluginRedirect);
    assert(!location->pluginRedirectedPath.has_value());

    identity.pluginRedirectedSaveExists = true;
    location = ResolveHomeMenuSaveLocation(identity, error);
    assert(location.has_value());
    assert(location->usesPluginRedirect);
    assert(location->activePath ==
        "fs:/vol/external01/wiiu/homebrew_on_menu_plugin/FW123456789/save/9234abcd/BaristaAccountSaveFile.dat");
    assert(location->pluginRedirectedPath.has_value());
    assert(location->mlcPath.find("storage_mlc:") == 0U);

    identity.systemMenuTitleId = 0x0005000010100000ULL;
    location = ResolveHomeMenuSaveLocation(identity, error);
    assert(!location.has_value());
    assert(!error.empty());

    identity.systemMenuTitleId = 0x0005001010040100ULL;
    identity.userPersistentId = 0U;
    location = ResolveHomeMenuSaveLocation(identity, error);
    assert(!location.has_value());

    identity.userPersistentId = 0x1234ABCDU;
    identity.consoleSerialId = "../bad-path";
    location = ResolveHomeMenuSaveLocation(identity, error);
    assert(!location.has_value());
    assert(!error.empty());

    std::cout << "HomeMenuSaveLocatorTests passed\n";
    return 0;
}
