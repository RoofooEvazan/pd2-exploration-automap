# Colors and reveal distance

Boundary, wall and water colors are independent. Reveal distance is fixed at **33 world subtiles**, with a smooth circular edge and quarter-subtile movement precision.

## Color lists

Independent **Boundary Color**, **Wall Color** and **Water Color** list pickers are available under **Options → Automap Options → Maps Styling** or **Campaign Styling**. Open the automap once in an offline game to initialize the extension. Click a color row, then click a named color or use Up/Down and Enter. The current color is marked **(Selected)**. Choosing a color saves it immediately and returns to its Styling submenu. **Back** or **Escape** returns without changing it. If saving fails, the picker stays open and the previous color remains active.

All three lists offer the thirteen colors below. Water Color also offers Light Blue (`#50A5DC`), its default. Each styling group saves its own `BoundaryColor`, `WallColor` and `WaterColor`; omitted keys inherit legacy preferences or default to Red, White and Light Blue respectively. [Stylization](map-styles.md) selects Original, Native, Hybrid or Styled independently for each group. Labels and values use native formatting, with larger headings in the color lists.

| Color | INI value | Base RGB |
| --- | --- | --- |
| Red | `red` | `#FF0000` |
| Vermilion | `vermilion` | `#E34239` |
| Orange | `orange` | `#FFA500` |
| Amber | `amber` | `#FFBF00` |
| Yellow | `yellow` | `#FFFF00` |
| Chartreuse | `chartreuse` | `#7FFF00` |
| Green | `green` | `#00FF00` |
| Teal | `teal` | `#008080` |
| Blue | `blue` | `#0000FF` |
| Violet | `violet` | `#7F00FF` |
| Purple | `purple` | `#800080` |
| Magenta | `magenta` | `#FF00FF` |
| White | `white` | `#FFFFFF` |

The hex values are the exact base colors. Boundary bands retain their existing shade multipliers and opacity; wall cores use the selected RGB with the existing opacity and dark contrast casing. Screen appearance therefore also depends on the game background and overlay transparency.

Wall Color changes Hybrid wall contours and straight sewer walls, plus Styled contours. Water Color changes recognized Hybrid shorelines, sewer water edges, riverbanks and grassy bank contours independently of Wall Color. It does not add new water geometry to Original, Native or Styled. Native artwork, icons, water fill and floor shading keep their colors. Original and Native retain native wall artwork, with Original's fullscreen white dimming described in [map styles](map-styles.md). Boundary Color also applies to fallback reveal accents.

Colors apply during drawing without geometry rebuilds or additional texture uploads; edge positions, width, alpha and exploration are preserved.

The extension supports the profiled expansion menu in `ProjectDiablo.dll` (SHA-256 `538A77B7CCEF3D5334E56C4E9E57A4D8FC69A1E27C46BEB694C0DEDFCFBF9CB3`) and documented D2Client/D2Win builds. The native engine handles navigation and owns the menu artwork; the picker uses native fonts and updates PD2's Escape target on transitions. Layout, pointers, drawing calls and Escape signatures are checked before installation. Unsupported menu versions retain INI color selection. No artwork or engine DLL is changed on disk. Live unloading is unsupported. Existing [screenshots](screenshots/) show the earlier five-color menu.

## Older color settings

Beta.9 replaces the earlier palette. Older INIs load without being rewritten. Retired names resolve as follows in either styling group:

| Old INI value | Current choice |
| --- | --- |
| `neon-green`, `pale-green` | Green |
| `cyan` | Teal |
| `light-blue`, `pale-blue` | Blue |
| `pale-yellow`, `pale-lemon` | Yellow |
| `pale-peach` | Orange |
| `gray`, `grey` | White |

Red, Orange, Magenta and White keep their names and use the new RGB values. Color names are case-insensitive. Choosing a color in the menu saves its current name to that group. Missing or invalid values inherit the legacy `[Automap]` preference, or default to Red for boundaries and White for walls. For `WaterColor`, `light-blue` selects the original `#50A5DC` tint; missing values retain that default.

## Boundary thickness

The **Boundary Thickness** row cycles through **0.5 → 1.0 → 1.5 → 2.0 → 0.5**. Each styling group saves its own `BoundaryThickness`. Changes update fullscreen and corner boundaries through the worker, even without movement; exploration and existing contours remain visible. The setting leaves wall/water width and reveal distance unchanged. A failed save retains the previous thickness.

Older shared values under `[Automap]` supply the default until a group saves its own value. Without either key, the default is 1.0. INI values between 0.5 and 2.0 remain supported; the next menu click advances to the next listed step. Manual edits require a restart.

## Fixed reveal distance

The runtime uses a compile-time constant of 132 quarter-subtile cells, equivalent to 33 world subtiles. There is no radius input in the INI or menu and no automatic resolution-based distance. Old `RevealRadiusSubtiles` and `RevealMode` keys are ignored, including oversized values; they can be removed when upgrading. Town-gate previews use the same compiled circle, and towns keep their existing full-exploration exemption.

The fixed radius prevents settings-based expansion, but an open-source client can still be modified. Online compatibility is not established. Discovery is distance-based and can reveal through nearby walls; it is not native tile discovery or line of sight.

## Validation

The runtime suite checks all thirteen RGB values and the additional Light Blue water default, independent saving/reload, thickness cycling and stationary rebuilds, legacy color aliases, defaults, failed saves, Back/Escape, layout bounds, native resource ownership and color/state restoration at both zooms. The fixed-distance suite uses legacy INI inputs to confirm the 33-subtile limit, resolution independence, movement, teleport gaps and town history. See [testing](testing.md) for commands and diagnostics.
