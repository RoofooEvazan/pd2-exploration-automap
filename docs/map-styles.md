# Map styles

Open the automap once, then choose **Options → Automap Options → Map Style**. Click the row, or select it with the keyboard and press Enter, to cycle **Native → Hybrid → Styled → Native**. One choice applies immediately to campaign areas and endgame maps, in both fullscreen and corner views.

| Choice | Appearance |
| --- | --- |
| Native | The game's original automap. No custom exploration clipping, floor shading, frontier, wall contours or entrance boost. Native discovery and normal icons draw as usual. |
| Hybrid | Modern contours, shaded floors and colored exploration frontier, together with native water patterns, roads, landmarks and icons. |
| Styled | Modern wall/shore contours, shaded floors and the exploration frontier, with original terrain sprites removed. Existing waypoints, shrines, events, portals, entrances, exits, stairs and other recognized navigation/object artwork are retained and clipped to explored space. |

Towns retain their fully revealed native appearance. The outdoor part of a town-gate preview follows the selected style. Boundary and Wall Color settings are shared by Hybrid and Styled and remain saved when using Native. The entrance visibility boost applies to retained eligible artwork in both custom styles. There are no added quest markers or exit arrows.

## Saved preferences and switching

The selection saves immediately as one key in `ExplorationMask.ini`:

```ini
[Automap]
MapStyle=hybrid
```

Accepted values are `native`, `hybrid` and `styled`. This setting overrides the older `CampaignStyle` and `MapsStyle` keys. Existing INIs without `MapStyle` continue to use their legacy group settings until a menu choice is saved. In those legacy keys only, `native` still means clipped native artwork, and `original` means the unmodified game map. The new **Map Style → Native** always means the original map, not that older intermediate mode. Invalid new values leave the legacy/default choices in effect. A failed save retains the current choice and displays **Save failed**.

Switching does not clear exploration, colors or completed geometry. Native keeps lightweight discovery history during automap updates so switching back retains the visited route, but it skips custom drawing, collision capture and geometry submissions. Rooms visited only in Native may still need capture when revisited in a custom style; no unloaded rooms are force-loaded. Hybrid and Styled share the existing geometry worker and caches. The reveal radius remains hardcoded to 33 world subtiles, with no menu or INI override.

## Styled details and limits

Styled uses the same fractional contour strokes and selected wall color as Hybrid. Its contours follow captured walkable-floor boundaries, including in sewers; it does not draw the sewer wall-face sprite or native water texture. That can give a different outline position from Hybrid's special sewer tracing. Roads, bridges and decorative terrain are omitted unless the active tables identify their artwork as a navigation feature or object.

Artwork referenced by objects, shrine/event definitions, recognized entrance labels and waypoint/landmark labels is protected. Shared entrance/terrain aliases are kept to avoid losing navigation, even when too ambiguous for the brightness boost. Purely decorative Arcane or explicitly fake stairs are not classified as entrances. This is table-based classification; unfamiliar custom labels can require refinement.

While geometry is first being prepared, the existing clipped-native fallback remains visible. Missing required tables or renderer support also retain native artwork instead of hiding navigation. Selection after a game started in Native loads any previously unnecessary style tables on the next automap update. The menu extension retains the native load/free ownership, Back/Escape behavior and label/value formatting.

## Validation

All three Win32 Release suites pass with optional local artwork/table/archive fixtures. Tests cover menu cycling and alignment, one-key persistence, legacy precedence, failed saves, unchanged colors, Native rendering bypass, discovery/cache preservation, both map zooms, campaign/endgame areas, hidden icons, terrain suppression, navigation artwork retention and warmup fallback. Existing clipping, water, worker, radius, color-menu and entrance-rendering tests remain included.

The [gameplay checklist](testing.md#gameplay-checks) covers style switching, icon retention, town gates and saved preferences. Broader icon coverage and sustained performance remain under test.
