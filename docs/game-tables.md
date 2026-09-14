# Game-table loading

The plugin reads definitions through the installed game's file resolver. The supported installation supplies `automap.txt`, `Objects.txt`, `Levels.txt`, `MonStats.txt` and `MonStats2.txt` from its archives or active direct overrides; no manual extraction is required.

## Behavior

`ExplorationRuntime.cpp` reads appearance settings during initialization and defers table loading until the first automap callback with a valid player. This avoids resolving base-game files before PD2 mounts its own archives. Opening another area or growing exploration does not repeat table I/O. Switching from Original can load previously unnecessary style tables. Restart after changing game files.

`GameTables.hpp` binds the already loaded native `Storm.dll`. It uses `SFileOpenFile` (ordinal 267), which consults the game's direct-access setting before delegating to `SFileOpenFileEx` and its mounted archive search. This preserves active `-direct` overrides without guessing archive names, priorities, or language settings. Calling `OpenFileEx` with scope zero alone would bypass direct overrides in this Storm build.

The current build requests `Levels.txt` and `Objects.txt` under `data/global/excel/`, plus `automap.txt` for Hybrid or Styled. Endgame icon support additionally requests `MonStats.txt` and `MonStats2.txt` through the same resolver. The reader uses the existing resolver without mounting archives or changing direct-access flags.

Every successful file open has an owner that closes the handle after parsing input is copied, including failed reads and C++ exceptions. Empty files, sizes over 16 MiB, nonzero high size words, failed reads and short reads are rejected. Owned input buffers are temporary; the existing compact classification/layer caches remain. Table parsers retain their row and schema limits.

Missing or malformed required artwork definitions make Hybrid and Styled use clipped-native fallback. Missing layer definitions retain separate area histories. Missing or malformed shrine/event definitions disable the affected icon fallback without disabling native drawing; see [shrine and event icons](map-markers.md). Original retains native artwork and discovery, including the appearance changes described in [map styles](map-styles.md). Each attempted file logs `TABLE <name>: ready; bytes=...; source=game file resolver` or a reason for failure; the `HYBRID`, `CAMPAIGN` and `STYLE` lines report parsing and selected behavior. A `ready` file read alone does not prove its schema was accepted.

## Compatibility

The inspected native Storm x86 module has timestamp `0x4B95C049`, image size `0x60000`, and SHA-256 `CC848EB2FC29068FB61CB258C1C8B9E8A191C5543805939762ED835B50FB194D`. The binder validates PE identity, export positions and selected instructions, including the relocated direct-access reference and OpenFileEx call. It rejects a different layout before calling it. The compatibility checker includes this module; no modified or separately downloaded Storm is required for an installation already using this supported build.

The four used exports are OpenFile 267 at `0x28DA0`, GetFileSize 265 at `0x262D0`, ReadFile 269 at `0x29BE0`, and CloseFile 253 at `0x26E20`. OpenFileEx 268 at `0x28960` is also validated as the resolver's delegate. These are native BOOL/stdcall signatures, not the standalone StormLib C++ API.

The installed PD2 archive tested here contains all three TXT tables. Other distributions might omit text tables or ship incompatible definitions; those cases retain the documented fallback. BIN-only data and unprofiled engine/renderer versions are unsupported.

## Validation

All three Win32 Release suites pass with local artwork and table fixtures. Synthetic checks cover ABI guards, changed exports/instructions, bounded reads, empty/oversized/high-word inputs, short and failed reads, exception cleanup, independent layer/artwork failure, style preservation and one-time loading.

An optional read-only check using private snapshots from the installed `pd2data.mpq` accepts all three tables: 132 campaign layer records and 115 ordinary wall IDs. The local direct overrides instead produce 111 ordinary wall IDs. Different counts are expected; active game files must determine classification. No snapshot belongs in the repository or release.

Offline archive/direct comparisons on 2026-09-12 both loaded and parsed the geometry/layer tables successfully. Archive reads produced 132 campaign entries and 115 ordinary wall IDs; the active direct overrides produced 132 and 111. These checks validate the resolver in that installation, not every table set.

For archive-only verification in a custom test installation, keep the normal test launch (including `-direct`) and add the temporary diagnostic argument `-exploration-archive-tables`. Only the plugin's table reads then use the already mounted archive search directly, via `SFileOpenFileEx` with null archive and scope zero. Other game data still follows the normal test setup. Do not remove `-direct` from a custom setup: its characters may depend on loose item definitions.

Enter an offline campaign area and open the automap. Check that all three TABLE reads report `source=mounted archives (table diagnostic)`, `HYBRID classified` and `CAMPAIGN layers` succeed, and hybrid style remains selected. Relaunch the normal test without this diagnostic argument afterward and verify the custom table counts and existing map appearance. No game file needs to be moved or deleted for this comparison. The diagnostic argument is not required for normal installation or automatic archive fallback.
