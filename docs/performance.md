# Performance

New terrain is built on one background worker. The game draws the latest completed geometry while the worker processes a replaceable pending snapshot. Drawing runs each automap pass; these optimizations do not skip frames or lower reveal resolution.

## Geometry caching

`StyledChunks.hpp` retains 64×64 fine-cell regions and rebuilds those affected by exploration or newly connected floor. The shade-filter halo is 14 fine cells at default boundary thickness and grows to 26 at maximum thickness. Half-open ownership prevents duplicate coverage at region boundaries, and adjacent matching quads are compacted before publication. Straight collinear wall fragments are joined without bridging gaps.

`PreparedFloors.hpp` prepares projected coordinates and bounds once per completed result. Per-frame rendering translates those coordinates for pan, rejects off-screen quads and clips viewport crossings. Both zooms retain double precision until submission. Preparation is capped at 65,536 quads, or 6 MiB of item payload per result. The render-side result, completed worker result and in-progress result can together hold up to 18 MiB of this payload, plus overhead. Larger drawings use the unprepared path.

## Artwork clipping and water reuse

`RasterClipCache.hpp` stores exact clipping rectangles in stable automap coordinates before pan. It has 8,192 slots with at most 16 rectangles each: up to 2 MiB of rectangle payload plus metadata and scratch space. Fully visible rectangles remain valid as the same mask grows; partial and hidden coverage refreshes. Area/session, zoom, town-footprint and mask-shrink changes invalidate the cache. All drawing paths share its invalidation context. Hash collisions and complex cells use the original query.

`SewerWater.hpp` uses reusable contiguous buffers and bounded duplicate detection. It reuses the perimeter when the sorted tile set is unchanged, including during pan. Changed tiles rebuild the perimeter. The 8,192-tile limit and native bridge exclusion still apply.

Poisoned Well's added translucent green fill is disabled. Hybrid still reads the referenced water pixels to classify shore contours, excluding dry objects and bridge pixels. Once a visible tile is registered in stable automap coordinates, repeated callbacks skip decoding and visibility queries. Original, Native and Styled do not run this classification path. Native outline colors remain available.

## Duplicate contours

In the tested Poisoned Well definitions, `PW wall` and `PW outline` identify contour sprites, including blank filler frames. Treating them as details drew native terrain beside the modern contour and duplicated clipping work.

Classification recognizes those labels in table group 46, scoped to level 202. The inspected tables contain 289 matching frame IDs. Object references and unknown aliases within the group protect a frame from replacement; aliases in other areas remain independent. Textured water is retained. If modern geometry is unavailable, native rendering returns. The classification uses a fixed 64 KiB array.

## Transparent sprite padding

Some native water/detail sprites have large transparent margins. `ArtworkBounds.hpp` validates DC6 RLE data and caches the occupied rectangle with a one-texel bilinear-sampling margin. Hybrid rendering uses it to avoid mask queries over empty space and to skip blank sprites. Sewer tracing and native towns retain the full-canvas path.

The metadata cache stays below 1 MiB, including scratch space. Keys include source file/frame identity, index, dimensions and encoded length; area/session changes invalidate it. The supported profile treats DC6 frames as immutable. Invalid metadata, failed reads or unsupported encoding use the original clipping path. Texture preparation and UVs remain native.

## Historical measurements

These measurements predate beta.7. They compare individual revisions, use different fixtures and must not be added together. Synthetic scenes use mocked native/Glide callbacks and measure CPU work, not game FPS or GPU time.

| Revision / scene | Before | After | Measurement |
| --- | ---: | ---: | --- |
| Incremental geometry, 1.5M fine cells | 35.23 ms | 6.46 ms | Mean build time across 24 small movements; 9.75 of 391 regions rebuilt on average. |
| Cached clipping, sewer | 7.15 ms | 5.67 ms | Median callback time per pass. |
| Cached clipping, town preview | 4.18 ms | 2.50 ms | Median callback time per pass. |
| Cached clipping, outdoor campaign | 2.72 ms | 2.22 ms | Median callback time per pass. |
| Prepared floors, Styled | 4.00 ms | 3.09 ms | Median callback time; 1.43M fine cells and 50,984 floor quads. |
| Prepared floors, Hybrid | 5.27 ms | 4.17 ms | Same fixture as the preceding row. |
| Poisoned Well contour suppression | 3.32 ms | 1.83 ms | Median callback time; 1.58M fine cells and 6,804 floor quads. |
| Occupied artwork bounds | 3.70 ms | 3.12 ms | Median callback time; 1.56M fine cells and 13,276 floor quads. |

Callback comparisons used three paired x86 Release runs of 180 passes each, with a panning viewport. Clipping/water counts and checksums matched. Prepared-floor output retained vertex coverage; joined wall runs fell from 1,481 to 1,356 with unchanged total length and polygon area. Preparation time is outside the callback measurements.

Poisoned Well native submissions fell from 1,439,100 to 141,840 per run while retained icon/water counts and modern geometry matched. The padding optimization reduced native submissions from 1,684,800 to 742,860 (55.9%); tests separately verify visible pixels and interpolated UVs. The fixtures use private installed artwork and tables, not distributed game assets.

An earlier v0.1.0 live trace sampled 25 updates beyond 1.4M explored fine cells: the latest worker build averaged 11.43 ms (4.58–20.45), and automap callback CPU time averaged 1.45 ms (1.12–1.61). The game displayed 240 FPS. A later Poisoned Well playtest reported virtually no frame drops after contour suppression. These are observations from one setup; repeated log samples may refer to the same completed worker result.

## Remaining costs

Snapshot comparison, assembly, projection and drawing still scale with retained geometry. Initial builds, large newly connected areas and evicted worker entries can require more work. Capture limits can leave later styled terrain unavailable. Individual caches are bounded, but total session memory is not globally capped; see [resource limits](architecture.md#lifetime-and-resource-limits).

Hybrid retains more native artwork than Styled. The style selector makes that tradeoff available without restarting. Color changes add no geometry rebuild; entrance brightness adds at most two palette uploads per pass, with no extra sprite draws.

Long-session stability and late-map FPS still need broader measurement. An earlier prototype hit a D2Glide texture-cache assertion with an unconfirmed cause. Current clipping prepares each native cell once, but that does not establish a fix for the earlier assertion.

For a comparison, record the version, style, renderer settings and area, then sample early and nearly complete exploration while moving and standing still. Test both map views. `mapMs` measures only the automap callbacks; worker timing and cache counters are described in [testing](testing.md).
