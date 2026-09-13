# Map styles

Open the automap once, then choose **Options → Automap Options → Maps Styling** or **Campaign Styling**. Each submenu has **Boundary Color**, **Wall Color**, and **Stylization**. Color rows open lists; Stylization cycles through the four styles. Choices apply immediately and save independently for each group, in both fullscreen and corner views.

| Choice | Appearance |
| --- | --- |
| Original | Unchanged game automap, without custom clipping, shading, frontier, contours or entrance emphasis. Called Native through beta.7. |
| Native | Original terrain artwork with exploration clipping and a colored boundary. No custom floor shading or wall contours. |
| Hybrid | Contours, shaded floors and a frontier alongside native water patterns, roads, landmarks and icons. |
| Styled | Contour terrain and shaded floors, retaining recognized waypoints, shrines, events, portals, entrances, exits and stairs. |

Towns retain native drawing without exploration clipping. Their outdoor preview uses Campaign Styling. Wall Color affects Hybrid walls and Styled contours. Recognized Hybrid water/bank edges use light blue. Original and Native keep the game's wall colors. The entrance visibility boost applies to recognized artwork in Hybrid and Styled. No quest markers or exit arrows are added.

## Saved preferences

```ini
[Automap]
OverlayOpacity=80
BoundaryThickness=1.0

[Maps]
Style=hybrid
BoundaryColor=cyan
WallColor=white

[Campaign]
Style=native
BoundaryColor=red
WallColor=gray
```

`BoundaryThickness` is a shared reveal-edge width multiplier from **0.5 to 2.0**. It scales the edge and its fade bands inward, with quarter-subtile rounding. It does not change exploration distance, wall contour width or icon size. Invalid values use 1.0. Manual edits take effect after restarting.

Legacy colors and styles in `[Automap]` supply defaults until the corresponding group key is saved. Legacy `MapStyle=native` keeps its old untouched-map meaning, now **Original**. In the new `[Maps]` and `[Campaign]` sections, `Style=native` means original artwork with exploration clipping and a boundary. A failed menu save keeps the current choice and displays **Save failed**.

Switching preserves exploration and completed geometry. Original records lightweight discovery history while skipping custom rendering and geometry work. Rooms visited only in Original may need capture when revisited in another style; unloaded rooms are never forced to load. The reveal radius stays hardcoded to **33 world subtiles**.

## Area transitions

New areas refresh loaded-room capture immediately, including transitions within a shared campaign layer. The boundary can draw before native map tiles arrive, using the native viewport and current explored mask. Unknown space may carry the reveal edge while data loads; only captured connected floor produces shading and wall contours. Town rectangles are excluded from the colored edge.

During the first worker update, a clipped boundary stroke covers startup outside towns. Native terrain remains until replacement floor data is ready. The stale-layer guard remains: new-area geometry cannot draw using the previous layer's transform.

## Styled details and limits

Styled uses floor-derived contours, including in sewers. Their positions can differ from Hybrid's artwork-aligned sewer tracing. Roads, bridges and decorative terrain are omitted unless active tables identify their artwork as navigation or an object. Shared entrance/terrain aliases are retained when needed to protect navigation. Unfamiliar custom labels may need classification updates.

Automated checks cover independent preferences, legacy migration, four styles, nested Back/Escape, save failures, both zooms/viewports, boundary-only startup, thickness limits, town exclusion and cached/full-build equivalence. Offline checks confirmed the entry-wall and blinking corrections. Broader coverage remains on the [gameplay checklist](testing.md#gameplay-checks).
