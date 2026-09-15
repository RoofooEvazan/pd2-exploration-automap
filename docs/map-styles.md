# Map styles

Open the automap once, then choose **Options → Automap Options → Maps Styling** or **Campaign Styling**. Each submenu has **Boundary Color**, **Wall Color**, **Water Color**, **Boundary Thickness** and **Stylization**. Colors open lists; Boundary Thickness and Stylization cycle through their choices. Preferences apply immediately and save independently for each group in both views.

| Choice | Appearance |
| --- | --- |
| Original | Native game automap and discovery, without custom clipping, frontier, wall contours or entrance emphasis. Called Native through beta.7. |
| Native | Original terrain artwork with exploration clipping and a colored boundary. No custom floor shading or wall contours. |
| Hybrid | Contours, shaded floors and a frontier alongside native water patterns, roads, landmarks and icons. |
| Styled | Contour terrain and shaded floors, retaining recognized waypoints, shrines, events, portals, entrances, exits and stairs. |

Towns retain native drawing without exploration clipping. Their outdoor preview uses Campaign Styling. Wall Color affects Hybrid walls and Styled contours. Recognized Hybrid water/bank edges follow Water Color, which defaults to White. Original dims neutral white fullscreen walls by 40%; colored pixels, protected icons and the corner minimap are unaffected. Native keeps the game's wall colors. The entrance visibility boost applies to recognized artwork in Hybrid and Styled. No quest markers or exit arrows are added.

## Default preferences

Maps default to Styled; Campaign defaults to Hybrid. Both use Cyan boundaries, White walls and water, 1.0 boundary thickness. Fullscreen opacity follows the existing Fade option.

See the [default INI](../ExplorationMask.ini) and [appearance settings](boundary-settings.md) for color values, thickness and Fade behavior.

`BoundaryThickness` is saved separately for Maps and Campaign. The menu cycles through **0.5, 1.0, 1.5 and 2.0**. It scales the edge and its fade bands inward, with quarter-subtile rounding. It does not change exploration distance, wall contour width or icon size. Missing or invalid group values inherit `[Automap] BoundaryThickness`, or default to 1.0. The active boundary rebuilds on the worker after a menu change, even while standing still. Manual edits take effect after restarting.

The existing Automap **Fade** setting controls fullscreen wall and water drawing across all four styles. Custom walls, water and floor shading follow its No, Center, Everything and Auto modes. Exploration boundaries keep their base opacity, and the corner minimap retains its fixed capture opacity. Legacy `StylizationOpacity` and `OverlayOpacity` keys are ignored.

Legacy colors and styles in `[Automap]` supply defaults until the corresponding group key is saved. Legacy `MapStyle=native` selects native artwork and discovery, now **Original**. In the new `[Maps]` and `[Campaign]` sections, `Style=native` means original artwork with exploration clipping and a boundary. A failed menu save keeps the current choice and displays **Save failed**.

Switching preserves exploration and completed geometry. Original records lightweight discovery history while skipping collision geometry work. Rooms visited only in Original may need capture when revisited in another style; unloaded rooms are never forced to load. The reveal radius stays hardcoded to **33 world subtiles**.

Poisoned Well's optional custom automap references pair existing water frames with outline frames. When those definitions are active, Original and Native tint the native water edge dull green (`#708860`). Hybrid uses the selected Water Color for shore contours; Styled omits native water artwork. The plugin adds no green water fill. These custom game definitions are not bundled with the plugin; installed native water patterns remain available without them.

## Area transitions

New areas refresh loaded-room capture immediately, including transitions within a shared campaign layer. The boundary can draw before native map tiles arrive, using the native viewport and current explored mask. Unknown space may carry the reveal edge while data loads; only captured connected floor produces shading and wall contours. Town rectangles are excluded from the colored edge.

During the first worker update, a clipped boundary stroke covers startup outside towns. Native terrain remains until replacement floor data is ready. The stale-layer guard remains: new-area geometry cannot draw using the previous layer's transform.

## Styled details and limits

Styled uses floor-derived contours, including in sewers. Their positions can differ from Hybrid's artwork-aligned sewer tracing. Roads, bridges and decorative terrain are omitted unless active tables identify their artwork as navigation or an object. Shared entrance/terrain aliases are retained when needed to protect navigation. Unfamiliar custom labels may need classification updates.

Automated checks cover independent preferences, legacy migration, four styles, nested Back/Escape, save failures, both zooms/viewports, boundary-only startup, thickness limits, town exclusion and cached/full-build equivalence. Offline checks confirmed the entry-wall and blinking corrections. Broader coverage remains on the [gameplay checklist](testing.md#gameplay-checks).
