# Campaign BETA guide

The campaign now defaults to a hybrid appearance: shaded floors and the red exploration edge from the modern map, gray wall/shore contours with a subtle dark border, and the original water patterns, roads, stairs, entrances, and icons. Ordinary wall artwork is selectively replaced; uncertain or mixed-purpose artwork stays native. No new quest markers, exit arrows, destination labels, or room-reveal calls are added.

This guide covers v0.2.0-beta.2, including automatic game-table loading. Offline testing confirmed the sewer appearance and adjoining-area persistence. The beta also includes clipping/water optimizations and a more translucent full-screen overlay. All three suites pass, including optional local artwork/table integration. Broad campaign coverage, sustained FPS and long-session stability remain under test; the beta label does not imply general compatibility.

## Settings

Copy [ExplorationMask.ini](../ExplorationMask.ini) beside `Game.exe`. Settings are read at startup; restart after editing them. Missing or invalid values use the defaults below.

```ini
[Automap]
CampaignStyle=hybrid
MapsStyle=styled
OverlayOpacity=80
```

| Value | Behavior |
| --- | --- |
| `hybrid` | Modern contours with a dark contrast border, native details/icons, shaded floors, and red exploration bands. Uses the active game tables described below. |
| `native` | Original artwork clipped to explored space, with shaded floors and red exploration bands. |
| `styled` | The previous simplified floor/wall appearance and exploration bands. |
| `original` | Native automap with the plugin's exploration mask and shading disabled for that group. |

The inspected level table identifies campaign levels as 1 through 132, ending at Worldstone Chamber. Special/endgame levels above 132 use `MapsStyle`. Towns retain their native footprint; nearby outdoor terrain follows its selected style on both sides of the gate. There are no keyboard bindings or in-game controls in this prototype.

`OverlayOpacity` accepts 10-100 and defaults to 80: custom floor shading, water fill, outlines and reveal bands use 80% of their previous alpha. Set 100 to restore the previous overlay opacity. RGB colors and native icons/artwork are preserved. The tested D2GL shader writes the corner map to a separate target with fixed alpha 0.9; that opacity is unaffected. This behavior was checked in the installed renderer's embedded shader and is specific to the supported renderer. Restart after changing the setting.

## Hybrid wall and shoreline rendering

On the first valid automap update, `HybridArtwork.hpp` receives `data/global/excel/automap.txt` and `Objects.txt` through the game's archive/direct-file resolver. Manual table extraction is no longer required. See [automatic table loading](game-tables.md). No tables or artwork are bundled here. Only recognized ordinary wall descriptions qualify for replacement. Roads, water/pools, bridges, stairs, doors, entrances, landmarks, ambiguous frame IDs, and every ID referenced by `Objects.txt` remain native. Missing or malformed required tables make the requested hybrid mode fall back to `native`.

The tested tables identify 111 ordinary wall frame IDs eligible for the floor-contour replacement. Six Act 3 sewer wall pieces (283 through 288) use a separate artwork-based route. The sewer drain/water frame 289 receives a faint fill and channel outline while retaining its native pattern; bridge frame 290, entrances, and icons remain native. Water classification is scoped to the `3 Sewer` table rows and levels 92/93 because unrelated endgame terrain reuses that frame ID. Conflicting sewer definitions or any object-icon reference prevent water styling. This is a tile-set exception, not a universal proof that every wall or water surface has a replacement.

In Act 3 sewer levels 92 and 93, the floor-contour wall pass is skipped because its positions differ from the visible wall faces. `NativeWallTrace.hpp` uses straight 2:1 isometric profiles with shared tile endpoints, selected by the known sewer wall forms. Each profile is checked against the native opaque pixels before use; unsupported artwork falls back to the original clipped sprite. This replaces the earlier pixel-center tracing, which followed decorative scallops and looked wobbly. A native wall is suppressed only after its gray core/dark casing has been queued successfully. Modern floor shading and red reveal bands remain.

`SewerWater.hpp` joins native water-tile diamonds and removes shared internal edges, leaving a channel perimeter. The runtime adds a faint slate fill (RGB 86,96,100; alpha 56) and the same gray/dark border used for walls. Native water artwork is still drawn; bridges are excluded. Water tiles are collected before exploration clipping so adjacent hidden tiles do not create false internal seams, but every new fill and full-width border is clipped to explored space and the viewport.

The wall cache owns its geometry and is capped at 24 entries; it retains no pointers into the native texture cache. Water collection is capped at 8,192 tiles per pass. At most three extra batches are submitted per automap pass: water fill, casing, and gray core, each limited to 65,536 vertices. Without water there are at most two. Unsupported walls or full wall batches retain native artwork; water limits can omit later highlights while preserving native water. These bounds do not limit the rest of the game process. Native artwork files, palettes, texture-cache metadata, and renderer binaries are never edited.

The frame index comes from the native draw context; the tested game's call path copies the automap cell ID directly into that context. Classification occurs once at startup, and rendering uses an array lookup. The table labels are descriptive data, never instructions. The data hashes in `compatibility.json` identify the inspected tables; the runtime parses the installed files rather than enforcing those hashes.

`cellHook` suppresses only eligible outdoor wall primitives when the styled geometry is ready. Native town portions remain visible. If styled geometry is unavailable, the existing native clipping fallback applies. Native details use the same primitive clipping as native mode. The exploration mask remains independent of the original `Wall1` through `Wall4` definitions.

The modern contour uses a 1.0-screen-pixel gray core (RGB 148,148,148; alpha 224) over a 2.5-pixel dark casing (RGB 24,24,24; alpha 192), with fractional vertices at both automap zooms. These are prototype constants, not INI settings. Both complete stroke widths are clipped to explored space and the viewport before two batched submissions. The existing background geometry cache is reused; no second floor rebuild or new game hook is added.

Outside the Act 3 sewer exception, shore outlines follow known walkable-floor boundaries where collision data supplies them and water retains its native pattern. The prototype does not identify every terrain material or guarantee a separate contour for every decorative water tile. Uncertain artwork may leave some native wall ornamentation beside the modern contour. That conservative choice protects campaign navigation details.

## Session persistence

Campaign areas sharing the same native automap layer now share one exploration mask, retained room capture, and incremental geometry cache, keyed by act, seed, and layer. For example, Spider Forest and Flayer Jungle remain visible together after crossing their zone boundary. The previous area does not need to remain loaded: owned room copies and completed geometry are retained. Returning to it selects the same state. Native artwork uses the shared clipping mask too, so its familiar details do not disappear at the crossing.

Different native layers and acts remain separate. `CampaignLayers.hpp` reads expected campaign layers from the user's local `data/global/excel/Levels.txt` at startup. The inspected table covers all 132 campaign areas. Shared keys require the native layer to match this expected layer; if the player area changes before the automap switches layers, the callback skips exploration mutation and geometry capture until they agree. This guard was added after a live trace showed sewer level 92 briefly paired with outdoor layer 57 before switching to 66.

Missing or malformed layer definitions disable shared keys, leaving separate per-area caches. A defined area whose native layer is unavailable or mismatched temporarily draws through the normal game path; it does not add that area to another layer's history. `Levels.txt` must match the active game's map definitions. Endgame maps retain their per-level keys. Town previews use the same shared outdoor key, while the town footprint stays fully visible on its own native layer. No extra rooms are loaded or automatically revealed. Only the current area's loaded rooms are newly captured; adjacent areas first seen across a border may still await capture when entered.

The two-second automap-gap reset has been removed. A process-lifetime watcher samples the player-unit pointer and native menu-control list every 50 ms. It publishes a menu epoch when controls are present and the player unit is absent. Missing room/path data alone does not qualify. Actual mask/cache changes remain on the automap thread, avoiding concurrent mutation.

Session identity tracks the menu epoch, player ID, and remembered seeds for each of the five acts. A new menu epoch, changed player ID, or changed seed for a previously visited act resets the session and records its reason. Cross-act travel, pauses, map visibility changes, and transient player-state read failures do not clear exploration by themselves. The renderer still rejects worker results from old sessions.

The menu reader adds a dependency on the tested `D2Win.dll`: SHA-256 `FF5CBB956F1A419E0E76FC2DF3BE21065BBBA894770B8DE7CCE8BC55EC453736`. It reads the control-list pointer at RVA `0x214A0`, checking the PE image identity and three relocated instruction references before binding it. No additional game hook is installed. If the layout does not match, or the watcher cannot start, the exploration effect does not activate.

## Validation and limits

All three Win32 Release suites pass. Hybrid regression checks cover conservative classification, object-ID conflicts, malformed tables, guarded reads, both zooms, clipped contrast strokes, preserved native details, town coverage, and native fallback. Sewer checks cover straight shared-endpoint profiles, native fallback for unsupported/budget-limited cells, cache reuse, bounded batch submission, no floor-contour duplicates, and retained shading. Water checks cover shared-edge removal, duplicate tiles, bridge exclusion, preserved native cells, fill/border clipping, and clearing between passes. Optional integration decoded both installed 1,974-frame artwork sheets; sampled points on all 12 sewer wall profiles were within two pixels of source artwork. A two-area worker test unloads the previous native room and verifies retained floor geometry and native-detail clipping, return travel, layer/act/endgame isolation, rejection of a stale native layer, invalid metadata fallback, and new-game clearing. See [testing.md](testing.md) for optional local integration checks.

Offline testing confirmed adjoining-area persistence and the straight sewer wall/water appearance. The performance revision retains that geometry. Updated Spider Forest and Poisoned Well screenshots show both overlay and corner modes; they are examples, not evidence of sustained FPS or long-session stability.

`RasterClipCache.hpp` caches exact clipping rectangles in coordinates before screen pan. Its 8,192 slots hold at most 16 rectangles each; hash collisions and complex cells use the original query. Exploration growth, session/layer changes, zoom changes and town-footprint changes invalidate coverage immediately. Water tiles use reusable contiguous storage, bounded duplicate detection and a cached perimeter rebuilt only when the tile set changes. Every frame still draws the current map; no update-rate reduction or geometry decimation is used. See [hybrid-performance.md](hybrid-performance.md) for benchmark scope and limits.

Live checks should cover Act 1 roads and landmarks, waypoints, shrines/wells, a cave entrance, Worldstone Keep stairs, Kurast Sewer water/platforms, and Act 5 cages. Repeat portal/waypoint travel and map hiding, then save/exit and start a new game to verify that exploration resets at the correct time. Compare both overlay and corner minimap, large-area frame times, and the existing Harrogath gate behavior.

The earlier native-mode live trace retained the session through Act 5, areas 204/205, and Act 1. It recorded resets only for the initial player and a subsequent menu epoch. The Act 1 outdoor samples reached 496,200 explored fine cells; automap callback CPU time averaged 2.04 ms across 80 samples and peaked at 3.07 ms. These measurements do not represent hybrid performance, total game FPS, or GPU cost. The user supplied a visual confirmation, not an explicit FPS measurement.

Preserving native artwork is not a guarantee that every PD2 marker uses this hook. Separately drawn markers need individual verification. Native detail clipping and contrast contours add render work, although eligible native wall cells are suppressed early. Campaign FPS and shoreline appearance need measurement. Each shared campaign layer has the existing 2,048-room / 2,000,000-collision-cell capture budget, now covering its connected areas together. Reaching that budget can omit later styled terrain; a whole-act long-session check is still needed. Existing cache/memory limits otherwise remain. Menu detection is sampled, not a game-exit callback; it depends on the verified native control list and should be checked through repeated game restarts.
