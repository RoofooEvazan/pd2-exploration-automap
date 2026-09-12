# Shrine and event icons — BETA

Endgame automaps retain native shrine and PD2 event symbols. A small fallback
also draws those symbols for already-loaded units whose locations are explored
but which have not yet been registered on the native automap. It works in the
native exploration, styled and hybrid map modes, in both map sizes.

The active game's `Objects.txt` identifies shrines by their shrine subclass and
positive `AutoMap` entry. The supported PD2 profile uses frame 1499 for event
objects and actors; `MonStats.txt` and `MonStats2.txt` supply the actor mappings.
This includes the existing event marker objects, altars, event NPCs and invasion
portals that have that native symbol. No game artwork or tables are bundled.
There are no new quest markers, exit arrows, boss pointers or chest markers.

The fixed 31-subtile exploration policy is unchanged. The fallback checks the
unit's world location against the existing mask, then clips the icon's native
textured primitives to explored pixels and the viewport. Walls, water, frontier
color and opacity retain their existing behavior. Campaign and town behavior
is unchanged.

## Drawing and limits

`MapMarkers.hpp` takes an owned snapshot of loaded room units every 200 ms on
the game thread. It reads no presets, loads no rooms, calls no reveal functions,
and writes no unit flags or native automap cells. Each sample is limited to 256
rooms, 4,096 units and 256 candidate markers. Invalid pointers are guarded. A
new snapshot replaces the old one; nothing accumulates with total map coverage.

The native unit flag indicating automap registration takes precedence, including
for moving events or substituted shrine symbols. The terrain hook also records
the native marker draws in each pass. Only a missing, unregistered icon is
submitted at the end of that pass, using the native cell context and original
renderer. This avoids duplicate icons and leaves texture ownership with the
game. The borrowed artwork pointer is limited to the current pass.

Native registered icons retain the game's lifetime and used-shrine behavior.
Fallback icons follow the latest loaded-unit snapshot: disappearance, room
unloading or an event actor's death removes that fallback within about 200 ms.
There is no additional persistent marker cache. Unregistered icons outside
loaded rooms are not discovered. Missing/malformed marker tables disable the
affected fallback while keeping the ordinary native drawing path. Events that
have no native marker definition are not assigned a guessed symbol.

## Validation and gameplay check

The runtime suite tests table mappings and exclusion of ordinary quest/exit
artwork, guarded room/unit reads, invalid pointers and cycles, sample expiry,
fixed-radius visibility, primitive clipping, native registration and draw
deduplication, invalid frame bounds, both sizes and all supported map styles.
Optional private fixtures validate the installed PD2 object and actor tables.

Build with the existing Windows/Win32 steps in the main README, then run CTest
in Release configuration. Close the game before replacing its plugin.

For the local gameplay check, enter an endgame map with a shrine or native event
marker. Approach with the automap open, switch between the overlay and corner
map, activate the shrine/event, and walk away and back. Confirm the native icon,
placement, visibility and FPS. `ExplorationMask.log` includes marker-definition
counts and `MARKERS sampled=... added=... alreadyNative=...` diagnostics.

v0.2.0-beta.6 ships the exact DLL confirmed working by the user on 2026-09-12. All three Win32 Release suites passed with the private artwork/table/archive fixtures. The live log accepted 95 object definitions and seven event actor definitions, and recorded both fallback submissions and native-registration/duplicate skips. Those counts reflect the active development tables; other supported table sets can differ. This is a functional check of the tested build, not a broad performance or compatibility guarantee.
