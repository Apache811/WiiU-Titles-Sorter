# Active Home Menu save location

The active Home Menu save is account-specific. Resolve the following identity
before attempting to read it:

1. Resolve Wii U Menu title ID through `_SYSGetSystemApplicationTitleId(0)`.
   The high word must be `0x00050010`.
2. Initialize ACT, obtain the active slot with `nn::act::GetSlotNo()`, then
   obtain its persistent ID with `nn::act::GetPersistentIdEx(slot)`.
3. Mount the required storage volumes and obtain the console serial identity
   from `MCP_GetSysProdSettings` (`code_id` followed by `serial_id`). The MCP
   settings structure must be 0x40-aligned, as required by the known working
   Menu Sort Aroma implementation.
4. Check whether the Homebrew On Wii U Menu redirected save exists at:

   `fs:/vol/external01/wiiu/homebrew_on_menu_plugin/<serial>/save/<user-dir>/BaristaAccountSaveFile.dat`

   where `<user-dir>` is `0x80000000 | persistentId`, formatted as eight
   lowercase hexadecimal digits.
5. If that redirected file exists, it is authoritative and must be read first.
   Otherwise use the MLC save:

   `storage_mlc:/usr/save/00050010/<menu-title-low>/user/<user-dir>/BaristaAccountSaveFile.dat`

`WiiUHomeMenuSaveLocator` implements only deterministic path resolution. It
performs no filesystem access and cannot establish whether a path exists. The
platform layer must pass `pluginRedirectedSaveExists=true` only after a
successful read-only existence check. Never delete the plugin copy and never
write one location while the plugin is actively redirecting the menu to the
other.

The reader must reject unexpected file sizes before parsing. The currently
analysed sample is exactly 0x1FC44 bytes, but the application must fail closed
on unknown sizes rather than truncate, pad, or rewrite them.

## Provenance

The path precedence and identity derivation follow the inspected Menu Sort
Aroma implementation and Homebrew On Wii U Menu save redirection behaviour.
This component intentionally does not copy Menu Sort's save-writing logic.


## Read-only acquisition layer (v0.9)

`WiiUHomeMenuSaveReader` separates save acquisition from the platform filesystem
implementation through `IReadOnlySaveFileSystem`. The adapter must implement
only `GetFileSize` and `ReadExact`; it must not create, truncate, or write files.

The reader performs these checks in order:

1. Requires the console serial identity so it can probe the possible redirected
   path. If serial identity is unavailable, it refuses to assume redirection is
   absent.
2. Probes the redirected path. `NotFound` permits the MLC path; any other probe
   I/O failure aborts the operation.
3. If the redirected file exists, it is selected as authoritative. No later
   size/read failure can trigger MLC fallback.
4. Checks the exact file size against `0x1FC44` before allocating the save buffer.
5. Reads exactly that many bytes into a temporary buffer, publishing the
   snapshot only after a successful complete read.

The current implementation does not parse the binary structure, mount Wii U
volumes, collect ACT/MCP identity, or include a concrete Wii U filesystem
adapter. The file-size signature is based on the analysed sample and is a
fail-closed compatibility gate, not proof that every firmware/save revision
uses that size.
