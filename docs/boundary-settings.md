# Boundary settings — BETA

The unpublished follow-up fixes the reveal radius in the plugin at **33 world subtiles**. Public beta.6 and the existing screenshots use 31. The same smooth circular edge and quarter-subtile movement precision are retained. The existing color menu remains available; distance settings from older builds are ignored.

## Color lists

The unpublished follow-up adds independent **Boundary Color** and **Wall Color** list pickers to **Options → Automap Options**. Open the automap once in an offline game to initialize the extension. Click either row, then click a named color or use Up/Down and Enter. The current color is marked **(Selected)**. Choosing a color saves it immediately and returns to Automap Options. **Back** or **Escape** returns without changing it. If saving fails, the picker stays open and the previous color remains active.

Both lists offer thirteen colors, including Gray and White. Boundary Color defaults to Red; Wall Color defaults to Gray so existing installations retain their previous wall appearance. Existing INIs do not need replacement. Choices save separately as `BoundaryColor` and `WallColor` in `ExplorationMask.ini`. A separate [Map Style](map-styles.md) row cycles Native, Hybrid and Styled for both campaign areas and maps. The Automap Options rows align their labels and values with the native settings; each color list has a larger centered heading above its compact choices.

| Color | INI value | Base RGB |
| --- | --- | --- |
| Red | `red` | `#A53028` |
| Neon Green | `neon-green` | `#1EDC0F` |
| Magenta | `magenta` | `#DC14DC` |
| Cyan | `cyan` | `#00D2DC` |
| Light Blue | `light-blue` | `#50A5DC` |
| Orange | `orange` | `#F8883C` |
| Pale Blue | `pale-blue` | `#CCF4F4` |
| Pale Yellow | `pale-yellow` | `#FCE874` |
| Pale Green | `pale-green` | `#C4FCB0` |
| Pale Peach | `pale-peach` | `#FCE4A4` |
| Pale Lemon | `pale-lemon` | `#FCFCC4` |
| Gray | `gray` | `#949494` |
| White | `white` | `#FFFFFF` |

The hex values are the exact base colors. Boundary bands retain their existing shade multipliers and opacity; wall cores use the selected RGB with the existing opacity and dark contrast casing. Gray preserves the previous brightness in each wall renderer. Screen appearance therefore also depends on the game background and overlay transparency.

Wall Color changes the plugin's modern wall and shoreline outlines in hybrid mode, its straight sewer outlines, and the matching contour strokes in styled mode. Retained native artwork, shrine/event icons, floor shading, and water fill keep their existing colors. Native/original modes do not gain replacement wall outlines from this setting. The fallback reveal accents also follow Boundary Color.

Colors are applied during drawing, with no new geometry rebuilds, texture uploads, mask storage, or worker work. The compiled reveal radius remains 33 and is never a menu or INI input.

The extension supports the profiled expansion menu in `ProjectDiablo.dll` (SHA-256 `538A77B7CCEF3D5334E56C4E9E57A4D8FC69A1E27C46BEB694C0DEDFCFBF9CB3`) and documented D2Client/D2Win builds. The native engine handles mouse/keyboard navigation. A resource-free picker uses a compact native font; the original automap table remains the sole owner of its artwork. Menu transitions update both the active selection and PD2's Escape target. Layout, pointers, drawing calls and Escape signatures are checked before installation. Unsupported menu versions retain INI color selection. No artwork or engine DLL is changed on disk. Live unloading is unsupported. Existing [screenshots](screenshots/) show the earlier five-color menu.

## Fixed reveal distance

The runtime uses a compile-time constant of 132 quarter-subtile cells, equivalent to 33 world subtiles. There is no radius input in the INI or menu and no automatic resolution-based distance. Old `RevealRadiusSubtiles` and `RevealMode` keys are ignored, including oversized values; they can be removed when upgrading. Town-gate previews use the same compiled circle, and towns keep their existing full-exploration exemption.

This removes the supported configuration override. It is not anti-tamper enforcement: an open-source client can be modified or replaced. Online use would require separate compatibility approval and controls appropriate to the server/client trust model. This change alone does not establish online support. Distance-based discovery can reveal through nearby walls and is not native tile discovery or line of sight.

## Build and validation

Use the [Win32 Release build steps](../README.md#build-from-source), then run all three CTest suites. The fixed-distance suite loads legacy INI entries through the real appearance-settings reader and confirms exact circular coverage, the 33-subtile outer limit, resolution independence, quarter-cell movement, teleport gaps and town history. Existing color/menu, geometry, clipping, water and worker tests remain included.

All three suites pass for the 33-subtile color-picker candidate with the optional local artwork/table/archive fixtures. The initial color pickers were approved in local gameplay; White and the revised menu formatting were also approved in local gameplay (menus look perfect). Automated checks cover exact RGBs, independent persistence, legacy defaults, save failure, picker Back/Escape, resource ownership, layout bounds, label/value alignment, heading fonts, both zooms, clipped geometry and native color restoration. Its log should report `DISCOVERY mode=hardcoded` and `radiusSubtiles=33.00`. The public beta.6 DLL retains the previously approved 31-subtile radius. Run the normal offline Test PD2 setup (`-3dfx -direct -exploration-test -log`). Keep a backup of the previous DLL and INI for comparison.
