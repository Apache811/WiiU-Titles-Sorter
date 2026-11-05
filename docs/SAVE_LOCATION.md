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


## Read, write and recovery responsibilities

`WiiUHomeMenuSaveReader` and the Wii U platform adapter separate portable validation from device filesystem operations. The reader validates the expected save size before publishing a snapshot and reads the selected authoritative path exactly. The platform adapter owns ACT/MCP identity acquisition, volume access, file operations and transaction recovery.

The same resolved active path is used by the read and write workflows. Before parsing at startup, the platform adapter checks for interrupted Restore and Apply transactions and recovers the preserved original where required. Manual backup, Restore and Apply are exposed through the platform service; write operations are initiated only through the corresponding confirmed UI actions.

The published v1.0.0 includes this acquisition and write integration. The maintainer completed final release validation on the Wii U.
