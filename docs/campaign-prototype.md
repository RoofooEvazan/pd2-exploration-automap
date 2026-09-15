# Campaign terrain

Hybrid combines contour walls and shaded floors with native roads, water, bridges, entrances and symbols. Styled uses floor-derived contours while retaining recognized navigation artwork. Both use the same exploration history as endgame maps. Choose a view through [Campaign Styling](map-styles.md); colors and opacity are described in the [main settings guide](../README.md#appearance-settings).

## Hybrid walls and shores

`HybridArtwork.hpp` classifies frame IDs from the active `automap.txt` and `Objects.txt`. Recognized ordinary walls can be replaced; roads, water, bridges, stairs, entrances, ambiguous frames and object references remain native. Definitions load through the [game file resolver](game-tables.md).

The default contour uses White (RGB 255,255,255; alpha 224) with a nominal 1.0-screen-pixel core over a 2.5-pixel dark casing (RGB 24,24,24; alpha 192). Wall Color changes wall cores; recognized water and bank cores use Water Color (White `#FFFFFF` by default, alpha 224). Thin strokes snap to a uniform grid that survives D2GL position rounding, with cached geometry to keep walking stable. Fullscreen walls, water and floors follow the native Fade option, while boundaries retain their base opacity; the corner minimap keeps its fixed opacity. The full stroke width is clipped to explored pixels and the viewport. Collision-derived shores retain native water patterns, but not every decorative water surface supplies a floor boundary.

In the inspected direct-override tables, 111 ordinary wall IDs qualify for floor-contour replacement. Other active tables can produce different counts. Classification is an array lookup during drawing. Native walls are suppressed only when replacement geometry is available; missing or malformed required tables use clipped-native fallback.

## Water and bank identification

Hybrid splits existing contours where recognized native water and Act 3 riverbank tiles meet them. Act 1 river sprites supply a single traced shore, including beside towns; overlapping collision contours are suppressed to avoid doubled edges. Stone walls retain Wall Color.

Act 1 raised grassy banks can have no automap symbol. For outdoor levels 2–7, `NativeFloorReader.hpp` identifies loaded `pond.dt1`, `puddle.dt1` and `swamp.dt1` floor tiles. Low blocked ground cells receive a bank tag, which follows the owned room snapshot and existing contour through the worker cache. Wall, visibility and missile barriers keep their architectural color. Tile reads occur during new-room capture, not every draw.

The same reader recognizes supported river-floor materials in endgame maps. Source water survives nonblocking decorative overlays, while dry parts of a tile and props on them retain their wall classification. Unrecognized decorative terrain retains its existing rendering.

## Act 3 sewers

Hybrid treats levels 92/93 separately because walkable-floor boundaries differ from visible wall faces. Six wall frames (283–288) use straight 2:1 isometric profiles with shared endpoints. `NativeWallTrace.hpp` checks each profile against opaque source pixels. Unsupported artwork or a full batch retains the native sprite; suppression happens only after a replacement is queued.

Frame 289 supplies sewer drain/water diamonds. `SewerWater.hpp` cancels shared edges to form a channel perimeter, then adds a slate fill (RGB 86,96,100; alpha 56), dark casing and a core using the selected Water Color (White by default). Native water patterns stay visible. Bridge frame 290 is excluded. Classification is scoped to `3 Sewer` table rows and levels 92/93 because other areas reuse these IDs; conflicting definitions or object references prevent water styling.

Water tiles are collected before exploration clipping so hidden neighbors do not produce false internal seams. Every fill and full-width border is then clipped to explored space and the viewport. The wall cache holds 24 owned profiles; water collection accepts 8,192 tiles per pass. Rendering groups fill, casing, wall cores and water cores into batches capped at 65,536 vertices each. Reaching the water limit can omit later highlights while retaining native water.

Styled uses floor-derived contours in sewers, without native wall faces or water patterns. Its outline can therefore differ from Hybrid's traced wall positions. Recognized stairs, entrances and objects remain. See [Styled limits](map-styles.md#styled-details-and-limits).

## Towns and adjoining areas

Towns retain native drawing within their level rectangles. Already-loaded outdoor terrain near a gate uses the selected style before the player's area ID changes. One neighbor is previewed at a time; no additional rooms are loaded.

Campaign areas sharing a native automap layer share their mask, owned room captures and geometry cache, keyed by act, seed and layer. Spider Forest and Flayer Jungle, for example, remain visible together after crossing the boundary even if earlier room data unloads. Endgame areas keep per-level keys.

`CampaignLayers.hpp` reads expected layers from `Levels.txt`. Player area and native layer can change on different callbacks; a mismatch defers mask mutation and collision capture until they agree. Missing layer definitions retain separate per-area histories. Already-loaded neighbors on the verified shared layer are captured before the area label changes. Completed contours remain visible during pending or failed updates; native artwork fills only uncovered terrain.

Exploration survives map hiding, portal/waypoint travel and temporary room/path failures. A confirmed menu return, changed player ID or changed seed in a previously visited act resets the session. Nothing is saved across process restarts. The guarded D2Win menu observer and capture budgets are documented in [architecture](architecture.md).

## Coverage

Offline checks have confirmed Act 1 native details, the Harrogath/Bloody Foothills gate, adjoining-area persistence, and straight sewer wall/water rendering. Automated checks cover both zooms, object/alias protection, exact clipping, water seams, fallback paths, shared histories and session resets. See [testing](testing.md) for fixture coverage and a gameplay checklist.

Unfamiliar labels may preserve native ornamentation beside a contour. Other decorative water and independently drawn markers need separate checks. Each shared campaign layer has a 2,048-room / 2,000,000-collision-cell capture budget; reaching it can omit later styled terrain. Whole-act coverage and sustained performance remain under test.
