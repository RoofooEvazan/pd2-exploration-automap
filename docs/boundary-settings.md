# Appearance settings and reveal distance

Boundary, wall and water colors are independent. Reveal distance is fixed at **33 world subtiles**, with a smooth circular edge and quarter-subtile movement precision.

## Color lists

Independent **Boundary Color**, **Wall Color** and **Water Color** list pickers are available under **Options → Automap Options → Maps Styling** or **Campaign Styling**. Open the automap once in an offline game to initialize the extension. Click a color row, then click a named color or use Up/Down and Enter. The current color is marked **(Selected)**. Choosing a color saves it immediately and returns to its Styling submenu. **Back** or **Escape** returns without changing it. If saving fails, the picker stays open and the previous color remains active.

All three lists offer the six colors below. Each styling group saves its own `BoundaryColor`, `WallColor` and `WaterColor`; omitted keys inherit legacy preferences or default to Cyan, White and White respectively. [Stylization](map-styles.md) selects Original, Native, Hybrid or Styled independently for each group. Labels and values use native formatting, with larger headings in the color lists.

| Color | INI value | Base RGB |
| --- | --- | --- |
| Red | `red` | `#FF0000` |
| Yellow | `yellow` | `#FFFF00` |
| Green | `green` | `#00FF00` |
| Cyan | `cyan` | `#00FFFF` |
| Magenta | `magenta` | `#FF00FF` |
| White | `white` | `#FFFFFF` |

The hex values are the exact base colors. Boundary bands retain their existing shade multipliers and opacity; wall cores use the selected RGB with the existing opacity and dark contrast casing. Screen appearance therefore also depends on the game background and overlay transparency.

Wall Color changes Hybrid wall contours and straight sewer walls, plus Styled contours. Water Color changes recognized Hybrid shorelines, sewer water edges, riverbanks and grassy bank contours independently of Wall Color. It does not add new water geometry to Original, Native or Styled. Native artwork, icons, water fill and floor shading keep their colors. Original and Native retain native wall artwork, with Original's fullscreen white dimming described in [map styles](map-styles.md). Boundary Color also applies to fallback reveal accents.

Colors apply during drawing without geometry rebuilds or additional texture uploads; edge positions, width, alpha and exploration are preserved.

The extension supports the profiled expansion menu in `ProjectDiablo.dll` (SHA-256 `538A77B7CCEF3D5334E56C4E9E57A4D8FC69A1E27C46BEB694C0DEDFCFBF9CB3`) and documented D2Client/D2Win builds. The native engine handles navigation and owns the menu artwork; the picker uses native fonts and updates PD2's Escape target on transitions. Layout, pointers, drawing calls and Escape signatures are checked before installation. Unsupported menu versions retain INI color selection. No artwork or engine DLL is changed on disk. Live unloading is unsupported. Existing [screenshots](screenshots/) show the earlier five-color menu.

## Older color settings

Older INIs load without being rewritten. Color names are case-insensitive, and the menu saves only current names.

| Old INI value | Current choice |
| --- | --- |
| `neon-green`, `pale-green` | Green |
| `light-blue`, `pale-blue` | Cyan |
| `pale-yellow`, `pale-lemon` | Yellow |
| `gray`, `grey` | White |

Vermilion, Orange, Amber, Chartreuse, Blue, Violet, Purple, Teal and Pale Peach are no longer supported choices. Invalid or missing group colors inherit a supported legacy `[Automap]` value; otherwise boundaries default to Cyan, and walls and water default to White. Light Blue is removed from the water list; an existing `light-blue` preference resolves to Cyan.

## Boundary thickness

The **Boundary Thickness** row cycles through **0.5 → 1.0 → 1.5 → 2.0 → 0.5**. Each styling group saves its own `BoundaryThickness`. Changes update fullscreen and corner boundaries through the worker, even without movement; exploration and existing contours remain visible. The setting leaves wall/water width and reveal distance unchanged. A failed save retains the previous thickness.

Older shared values under `[Automap]` supply the default until a group saves its own value. Without either key, the default is 1.0. INI values between 0.5 and 2.0 remain supported; the next menu click advances to the next listed step. Manual edits require a restart.

## Native Fade

Use the existing **Fade** row in Automap Options to control fullscreen walls and water in all four styles. Original and Native retain the game's artwork fading. Hybrid and Styled apply the same modes to their custom walls, water edges and floor shading:

| Fade | Fullscreen behavior |
| --- | --- |
| No | Retains each layer's base alpha. |
| Center | Fades the area around the screen center in the native 25%, 50% and 75% bands. |
| Everything | Applies the native half-opacity multiplier throughout. |
| Auto | Uses the native three-quarter opacity while standing or walking; other animations retain base alpha. |

Center follows the native logical screen dimensions and shifts with side panels. Custom geometry is split only where it crosses a fade band, without rebuilding exploration or terrain. No, Everything and Auto retain the existing geometry and submission count. Center uses four alpha groups with bounded scratch storage.

Exploration boundaries keep their base opacity in every Fade mode. The corner minimap keeps D2GL's fixed capture opacity. Protected icons retain their native behavior and the existing entrance emphasis; Original's white-wall dimming remains. There are no separate opacity controls in Maps Styling, Campaign Styling or the INI. Old `StylizationOpacity` and `OverlayOpacity` keys are ignored and can be removed.

## Fixed reveal distance

The runtime uses a compile-time constant of 132 quarter-subtile cells, equivalent to 33 world subtiles. There is no radius input in the INI or menu and no automatic resolution-based distance. Old `RevealRadiusSubtiles` and `RevealMode` keys are ignored, including oversized values; they can be removed when upgrading. Town-gate previews use the same compiled circle, and towns keep their existing full-exploration exemption.

The fixed radius prevents settings-based expansion, but an open-source client can still be modified. Online compatibility is not established. Discovery is distance-based and can reveal through nearby walls; it is not native tile discovery or line of sight.

## Validation

The runtime suite checks all six RGB values and the Cyan/White defaults, independent saving/reload, thickness cycling and stationary rebuilds, legacy color aliases, defaults, failed saves, Back/Escape, layout bounds, native resource ownership and color/state restoration at both zooms. Appearance-default checks verify the palette and styles and ensure retired opacity settings cannot change rendering. The fixed-distance suite uses legacy INI inputs to confirm the 33-subtile limit, resolution independence, movement, teleport gaps and town history. See [testing](testing.md) for commands and diagnostics.
