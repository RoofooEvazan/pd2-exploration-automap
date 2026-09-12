# Automatic game-table loading — BETA

v0.2.0-beta.1 could initialize successfully yet lose hybrid styling and connected-area sharing when loose TXT files are missing. This source revision reads those definitions through the installed game's file resolver. Testers with the supported PD2 installation do not need to find or extract `automap.txt`, `Objects.txt` or `Levels.txt`.

## Behavior

`ExplorationRuntime.cpp` reads appearance settings during initialization and defers table loading until the first automap callback with a valid player. This avoids resolving base-game files before PD2 mounts its own archives. Tables are read and parsed once per process; opening another area or growing the explored map does not repeat file access. Restart after changing game files.

`GameTables.hpp` binds the already loaded native `Storm.dll`. It uses `SFileOpenFile` (ordinal 267), which consults the game's direct-access setting before delegating to `SFileOpenFileEx` and its mounted archive search. This preserves active `-direct` overrides without guessing archive names, priorities, or language settings. Calling `OpenFileEx` with scope zero alone would bypass direct overrides in this Storm build.

The current build requests `Levels.txt` and `Objects.txt` under `data/global/excel/`, plus `automap.txt` for hybrid styling. Endgame icon support additionally requests `MonStats.txt` and `MonStats2.txt` through the same resolver. These tables come from the installed game; testers do not need to download or extract them. The reader never loads an extra archive DLL, opens or remounts an MPQ, enables direct access, writes a file, downloads data, or changes native reveal/geometry definitions. No game tables are redistributed.

Every successful file open has an owner that closes the handle after parsing input is copied, including failed reads and C++ exceptions. Empty files, sizes over 16 MiB, nonzero high size words, failed reads and short reads are rejected. Owned input buffers are temporary; the existing compact classification/layer caches remain. Table parsers retain their row and schema limits.

Missing or malformed artwork definitions change requested `hybrid` styles to `native`. Missing layer definitions retain separate area histories. Missing or malformed shrine/event definitions disable the affected icon fallback without disabling native drawing; see [shrine and event icons](map-markers.md). `original` and `styled` selections are preserved. Each attempted file logs `TABLE <name>: ready; bytes=...; source=game file resolver` or a reason for failure; the `HYBRID`, `CAMPAIGN` and `STYLE` lines report parsing and selected behavior. A `ready` file read alone does not prove its schema was accepted.

## Compatibility

The inspected native Storm x86 module has timestamp `0x4B95C049`, image size `0x60000`, and SHA-256 `CC848EB2FC29068FB61CB258C1C8B9E8A191C5543805939762ED835B50FB194D`. The binder validates PE identity, export positions and selected instructions, including the relocated direct-access reference and OpenFileEx call. It rejects a different layout before calling it. The compatibility checker includes this module; no modified or separately downloaded Storm is required for an installation already using this supported build.

The four used exports are OpenFile 267 at `0x28DA0`, GetFileSize 265 at `0x262D0`, ReadFile 269 at `0x29BE0`, and CloseFile 253 at `0x26E20`. OpenFileEx 268 at `0x28960` is also validated as the resolver's delegate. These are native BOOL/stdcall signatures, not the standalone StormLib C++ API.

The installed PD2 archive tested here contains all three TXT tables. Other distributions might omit text tables or ship incompatible definitions; those cases retain the documented fallback. This removes manual extraction for the supported installation, not the existing engine/renderer compatibility requirement. BIN-only data and arbitrary PD2 versions are not newly supported.

## Validation

All three Win32 Release suites pass with the existing local artwork and table fixtures. New synthetic tests cover ABI guards, changed exports/instructions, bounded reads, empty/oversized/high-word inputs, short and failed reads, exception cleanup, independent layer/artwork failure, style preservation and one-time loading.

An optional read-only check using private snapshots from the installed `pd2data.mpq` accepts all three tables: 132 campaign layer records and 115 ordinary wall IDs. The local direct overrides instead produce 111 ordinary wall IDs. Different counts are expected; active game files must determine classification. No snapshot belongs in the repository or release.

Both live loader tests succeeded on 2026-09-12 in the local development build. The archive-only table test kept the normal custom game data loaded; native Storm returned Levels (113,855 bytes), automap (229,666 bytes) and Objects (231,713 bytes). All parsed successfully, with 132 campaign entries, 115 ordinary wall IDs and hybrid mode retained. The normal Test PD2 launch then read the direct overrides (119,428 / 257,133 / 234,656 bytes), retaining 132 entries and 111 ordinary wall IDs. The user confirmed both tests worked. The beta.2 release build uses the same table loader and passes all three suites; this does not establish broad map coverage or sustained FPS.

For archive-only verification in a custom test installation, keep the normal test launch (including `-direct`) and add the temporary diagnostic argument `-exploration-archive-tables`. Only the plugin's table reads then use the already mounted archive search directly, via `SFileOpenFileEx` with null archive and scope zero. Other game data still follows the normal test setup. Do not remove `-direct` from a custom setup: its characters may depend on loose item definitions.

Enter an offline campaign area and open the automap. Check that all three TABLE reads report `source=mounted archives (table diagnostic)`, `HYBRID classified` and `CAMPAIGN layers` succeed, and hybrid style remains selected. Relaunch the normal test without this diagnostic argument afterward and verify the custom table counts and existing map appearance. No game file needs to be moved or deleted for this comparison. The diagnostic argument is not required for normal installation or automatic archive fallback.

Build using the [main instructions](../README.md#build-from-source). Install the resulting DLL only with the game closed. Existing INI and D2GL loader settings do not need a new entry. Keep a copy of the previous DLL for rollback. v0.2.0-beta.2 contains this fix. Beta.3 retains this loader and also includes the [map performance changes](map-growth-optimization.md) and [boundary settings](boundary-settings.md).
