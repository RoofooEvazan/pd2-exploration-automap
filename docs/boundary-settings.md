# Colors and reveal distance

Boundary and wall colors are independent. Reveal distance is fixed at **33 world subtiles**, with a smooth circular edge and quarter-subtile movement precision.

## Color lists

Independent **Boundary Color** and **Wall Color** list pickers are available under **Options → Automap Options → Maps Styling** or **Campaign Styling**. Open the automap once in an offline game to initialize the extension. Click either row, then click a named color or use Up/Down and Enter. The current color is marked **(Selected)**. Choosing a color saves it immediately and returns to its Styling submenu. **Back** or **Escape** returns without changing it. If saving fails, the picker stays open and the previous color remains active.

Both lists offer thirteen colors, including Gray and White. Each styling group saves its own `BoundaryColor` and `WallColor`; omitted keys inherit legacy preferences or the Red/Gray defaults. [Stylization](map-styles.md) selects Original, Native, Hybrid or Styled independently for each group. Labels and values use native formatting, with larger headings in the color lists.

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

The hex values are the exact base colors. Boundary bands retain their existing shade multipliers and opacity; wall cores use the selected RGB with the existing opacity and dark contrast casing. Gray uses each wall renderer's default brightness. Screen appearance therefore also depends on the game background and overlay transparency.

Wall Color changes Hybrid wall contours and straight sewer walls, plus Styled contours. Recognized Hybrid water and bank edges use Light Blue (`#50A5DC`) independently of this choice. Native artwork, icons, water fill and floor shading keep their colors. Original and Native retain the game's walls. Boundary Color also applies to fallback reveal accents.

Colors are applied during drawing without geometry rebuilds or additional texture uploads.

The extension supports the profiled expansion menu in `ProjectDiablo.dll` (SHA-256 `538A77B7CCEF3D5334E56C4E9E57A4D8FC69A1E27C46BEB694C0DEDFCFBF9CB3`) and documented D2Client/D2Win builds. The native engine handles mouse/keyboard navigation. A resource-free picker uses a compact native font; the original automap table remains the sole owner of its artwork. Menu transitions update both the active selection and PD2's Escape target. Layout, pointers, drawing calls and Escape signatures are checked before installation. Unsupported menu versions retain INI color selection. No artwork or engine DLL is changed on disk. Live unloading is unsupported. Existing [screenshots](screenshots/) show the earlier five-color menu.

## Boundary thickness

Set `BoundaryThickness` under `[Automap]` to scale the reveal edge from 0.5 to 2.0. The default is 1.0. This setting applies to both styling groups and map sizes; it leaves wall width and reveal distance unchanged. Restart after editing the INI.

## Fixed reveal distance

The runtime uses a compile-time constant of 132 quarter-subtile cells, equivalent to 33 world subtiles. There is no radius input in the INI or menu and no automatic resolution-based distance. Old `RevealRadiusSubtiles` and `RevealMode` keys are ignored, including oversized values; they can be removed when upgrading. Town-gate previews use the same compiled circle, and towns keep their existing full-exploration exemption.

The fixed radius prevents settings-based expansion, but an open-source client can still be modified. Online compatibility is not established. Discovery is distance-based and can reveal through nearby walls; it is not native tile discovery or line of sight.

## Validation

The runtime suite checks all thirteen RGB values, independent saving/reload, defaults, failed saves, Back/Escape, layout bounds, native resource ownership and color/state restoration at both zooms. The fixed-distance suite uses legacy INI inputs to confirm the 33-subtile limit, resolution independence, movement, teleport gaps and town history. See [testing](testing.md) for commands and diagnostics.
