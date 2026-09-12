# Boundary settings — BETA

v0.2.0-beta.3 adds a color choice to the supported game's existing **Options → Automap Options** menu and keeps the rounded exploration mask while sizing it from the native game view.

## Color

Open the automap once after entering an offline game to initialize the menu extension. Open **Options → Automap Options**, then click **Boundary Color** (or select it with the keyboard and press Enter) to cycle through Red, Neon Green, Magenta, Cyan and Light Blue. The change applies immediately and saves `BoundaryColor` in `ExplorationMask.ini`. Native icons, gray walls, water and overlay opacity retain their own settings.

The menu prototype supports the profiled expansion menu in `ProjectDiablo.dll` (SHA-256 `538A77B7CCEF3D5334E56C4E9E57A4D8FC69A1E27C46BEB694C0DEDFCFBF9CB3`) with the documented D2Client/D2Win builds. It checks menu layouts, pointer references and the drawing call before installing. An unknown menu retains INI color selection. It adds one native text row; no DC6 assets or engine binaries are modified on disk. Live unloading is unsupported.

## Rounded reveal distance

`RevealMode=native-average` is the new default. Native D2Client discovery follows terrain rendering: its automap tests the terrain-drawn flag `0x20000`, rather than a fixed circular distance. A perfect native footprint would therefore reveal in tile-shaped steps and vary by terrain artwork.

At the user's request, the plugin preserves its existing **circular** mask. It reads D2Client's logical width and height, transforms the centered view into world coordinates with the native isometric projection, and averages the radial distance over all headings. That average sets the circle radius, rounded to a quarter-subtile. It uses the game's logical view, not the upscaled D2GL desktop window size. The calculation runs only when the logical dimensions change; it does not scan terrain tiles or grow with the amount already explored.

This is an **approximation of native view reach**, not exact native tile discovery, line of sight, or a per-tile reveal guarantee. A circle cannot match the native footprint in every direction. Camera shifts, panels and the artwork's size can also change native tile visibility. The existing fractional rendering and seven shade bands remain; no tile outlines are introduced. Towns remain fully revealed, and town-gate previews use the same circle radius as the outdoors.

`RevealMode=circle` restores the old fixed 20-subtile radius. Invalid/loading dimensions retain the last valid average, or 20 subtiles until a valid view is available. Existing exploration never shrinks when the view becomes smaller. Restart after changing RevealMode in the INI; color changes through the menu need no restart.

## Build and validation

Use the [Win32 Release build steps](../README.md#build-from-source), then run all three CTest suites. Current automated checks cover the five colors, persistence/save failure, native menu layout guards and resource routing, untouched non-frontier colors, and both render paths. Distance checks compare the average against independent angular sampling and exercise circular coverage, quarter-cell movement, resolution changes, teleports, towns and retained exploration. Existing tests retain the fixed-radius mode's contracts.

The user approved the local test on 2026-09-12. Live logs confirmed menu installation, saved selections for all four alternate colors, and a 28-subtile radius at 1068×600 logical resolution; a rounded magenta frontier was observed in Dark Temple. The release ships that exact tested DLL. Broader menu/restart coverage and sustained FPS still need testing. Run the normal offline Test PD2 setup (`-3dfx -direct -exploration-test -log`). The log reports the menu installation result, logical view dimensions, circle radius and color saves. Keep a backup of the previous DLL and INI for comparison.
