# Boundary settings — BETA

This unpublished follow-up fixes the reveal radius in the plugin at **31 world subtiles**. The same smooth circular edge and quarter-subtile movement precision are retained. The existing color menu remains available. The public beta.4 download still contains the earlier configurable build until this follow-up is approved for release.

## Color

Open the automap once after entering an offline game to initialize the menu extension. Open **Options → Automap Options**, then click **Boundary Color** (or select it with the keyboard and press Enter) to cycle through Red, Neon Green, Magenta, Cyan and Light Blue. Each selection applies immediately and saves `BoundaryColor` in `ExplorationMask.ini`. Native icons, gray walls, water and overlay opacity retain their own appearance.

The menu supports the profiled expansion menu in `ProjectDiablo.dll` (SHA-256 `538A77B7CCEF3D5334E56C4E9E57A4D8FC69A1E27C46BEB694C0DEDFCFBF9CB3`) with the documented D2Client/D2Win builds. Layout, pointer and drawing-call guards run before installation. An unknown menu retains INI color selection. No DC6 assets or engine binaries are modified on disk. Live unloading is unsupported. See the [screenshot gallery](screenshots/) for the menu and all five colors.

## Fixed reveal distance

The runtime uses a compile-time constant of 124 quarter-subtile cells, equivalent to 31 world subtiles. There is no radius input in the INI or menu and no automatic resolution-based distance. Old `RevealRadiusSubtiles` and `RevealMode` keys are ignored, including oversized values; they can be removed when upgrading. Town-gate previews use the same compiled circle, and towns keep their existing full-exploration exemption.

This removes the supported configuration override. It is not anti-tamper enforcement: an open-source client can be modified or replaced. Online use would require separate compatibility approval and controls appropriate to the server/client trust model. This change alone does not establish online support. Distance-based discovery can reveal through nearby walls and is not native tile discovery or line of sight.

## Build and validation

Use the [Win32 Release build steps](../README.md#build-from-source), then run all three CTest suites. The fixed-distance suite loads legacy INI entries through the real appearance-settings reader and confirms exact circular coverage, the 31-subtile outer limit, resolution independence, quarter-cell movement, teleport gaps and town history. Existing color/menu, geometry, clipping, water and worker tests remain included.

All three suites pass with the optional local artwork/table/archive fixtures. The prior 31-subtile appearance was user-approved; this hardcoded follow-up awaits a local game check. Run the normal offline Test PD2 setup (`-3dfx -direct -exploration-test -log`). The log should report `DISCOVERY mode=hardcoded` and `radiusSubtiles=31.00`. Keep a backup of the previous DLL and INI for comparison.
