# BETA hybrid performance and overlay update

v0.2.0-beta.1 includes the following optimizations after offline testing found occasional 15-25 FPS drops. It also provides adjustable full-screen translucency while retaining the straight sewer walls and water channels.

## Changes

Exact artwork clipping is reused in coordinates before screen pan. Cache coverage is invalidated immediately when exploration grows or the session, layer, zoom or town footprint changes. Hash collisions and complex cells use the original raster query. The cache has 8,192 slots with at most 16 rectangles each: at most 2 MiB of cached rectangle payload plus entry metadata, allocator overhead and query scratch space. It retains no game pointers as artwork data.

Sewer water uses reusable contiguous buffers and a bounded duplicate table instead of allocating tree nodes for every tile and edge on every frame. Its perimeter is reused when the sorted tile set is unchanged, including during screen panning. Changed tiles rebuild the perimeter. The 8,192-tile limit, native water pattern, bridge exclusion and existing geometry batch limits are retained.

Drawing still runs each automap pass. Reveal resolution, outline shapes, water-channel geometry and visibility rules are unchanged. There is no frame skipping or reduced minimap update rate.

`OverlayOpacity=80` in `ExplorationMask.ini` applies 80% of the previous alpha to custom shading, water fill, outlines and reveal bands. Values 10-100 are accepted; 100 restores the previous opacity. Native symbols and artwork keep their own rendering. The tested D2GL shader uses a separate corner-map target with fixed alpha 0.9, verified in the installed binary, so the corner minimap retains its opacity. Restart after changing the setting.

## Measurements

A local x86 Release benchmark ran the old and new renderer code against the same synthetic scene, with mocked native/Glide callbacks, a panning viewport, native clipping, floor quads and wall/water drawing. Each sample rendered 180 passes; the table gives the median of three runs per version.

| Scene | Before, ms/pass | After, ms/pass | CPU reduction |
| --- | ---: | ---: | ---: |
| Sewer | 7.15 | 5.67 | 21% |
| Town with outdoor preview | 4.18 | 2.50 | 40% |
| Outdoor campaign | 2.72 | 2.22 | 19% |

Native quad counts, custom vertex counts and geometry checksums matched between versions in all benchmark scenes. This measures the mocked CPU rendering path, not actual game FPS, GPU time, combat, room capture or long-session performance. It mainly measures reuse between exploration changes; rapidly growing exploration causes more cache misses. The prior live log is retained privately for comparison. Broader in-game measurements are still required; the refreshed screenshots do not establish sustained FPS.

All three Win32 Release suites passed with both optional local table and artwork integrations enabled. New regressions compare exact clipping coverage across cache hits, holes, negative panning, zoom changes, town unions, mask growth, collisions and complex-cell fallback. Water output is compared to an independent edge-set reference across reordered frames, changed tiles, duplicate tiles and capacity limits. Opacity checks verify RGB preservation and native color restoration.

Final release review found that distinct wall/cell drawing paths could remember different invalidation contexts for the same cache. The BETA shares that context across all callers. A regression reproduced stale coverage when changing areas between drawing paths and now passes. The benchmark above was rerun after this correction; this final correction has synthetic coverage but has not received a separate live-game session.

## Test in game

Use the existing test launch flags. Explore the sewers and revisit a well-explored area; compare FPS while walking and standing still, and check the town/outdoor boundary. Toggle between the full-screen overlay and corner minimap to check translucency and preserved icons. The log's `clipHits`/`clipMisses` show reuse; `mapMs` remains an automap CPU measurement, not total frame time.

Build/run instructions and rollback considerations remain in the README and [campaign BETA guide](campaign-prototype.md). No game assets, renderer binaries, engine DLLs, FPS caps or VSync settings are changed.
