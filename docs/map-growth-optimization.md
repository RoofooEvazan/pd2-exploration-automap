# Large-map performance and hybrid endgame styling

Included in v0.2.0-beta.3, following the beta.1 rendering baseline. Both `CampaignStyle` and `MapsStyle` now default to `hybrid`. Existing INI values remain authoritative, so an upgrade must change `MapsStyle=styled` to `MapsStyle=hybrid` to select the new map appearance. `OverlayOpacity=80` is retained. No markers or arrows are added.

## Why this changed

The latest Dark Temple log reached about 1.65 million explored fine cells. Near the end, automap callback CPU time was roughly 3.9-4.1 ms, enough to consume most of the 4.17 ms budget for 240 FPS. That timing includes the automap callbacks and drawing, not the whole game or GPU work. The user reported a 30-40 FPS loss as exploration grew.

## Rendering changes

- The existing worker prepares floor projection and per-quad bounds once for each completed geometry result. Every frame still draws the current map, translating the prepared coordinates for pan and clipping crossing polygons with the same clipper. Both zooms retain double precision until Glide submission.
- Prepared floor storage is capped at 65,536 quads, or 6 MiB of item payload per result. Only one prepared result is retained on the render side, rather than one per visited area. The worker can hold another completed result and one being built: up to 18 MiB of prepared payload, plus vector/object overhead. Larger drawings use the original rendering path without losing terrain.
- Straight collinear wall fragments are joined on the worker. Actual gaps stay open; there is no outline simplification, reduced exploration precision, or skipped frame.
- Fully explored clipping rectangles stay valid when the same mask grows. Partial and hidden rectangles refresh immediately. Area/session, zoom, town-footprint and mask-shrink changes still invalidate the cache completely. Its existing slot/rectangle limits remain.
- The same hybrid rendering used by campaign areas now applies to maps: modern shaded terrain and contrast outlines with recognized native water, details and icons. Endgame artwork can require further visual checks; the classification preserves uncertain artwork.

The runtime log adds `DRAW style`, `floorQuads`, `wallRuns`, and `preparedFloors` so a test can verify the selected style and prepared path. Existing `mapMs`, worker timings and clipping hit/miss counts remain.

## Validation

All three Win32 Release suites pass, including optional checks against the local game tables and both automap artwork sheets. Added checks compare prepared floor vertices and order with the original projection/clipper at both zooms, negative/fractional pans, distant world coordinates, clipped/empty viewports and all shade colors. They verify the preparation limit's fallback, exact wall coverage after joining, full-clip reuse during growth, partial/hidden refresh, mask shrink, and hybrid wall/detail rendering in campaign and endgame levels.

A synthetic scene contains 1,427,872 explored fine cells, 107,094 connected floor cells and 50,984 floor quads. A panning viewport renders 180 passes while exploration continues elsewhere. Native/Glide callbacks are mocked; the table is the median of three paired x86 Release runs against the published beta source.

| Style | Before, ms/pass | After, ms/pass | CPU reduction |
| --- | ---: | ---: | ---: |
| Styled | 4.00 | 3.09 | 23% |
| Hybrid | 5.27 | 4.17 | 21% |

Floor vertex counts and native drawing counts match. Wall joining reduces source runs from 1,481 to 1,356; total line length and submitted polygon area match. Exact wall and clipping coverage are checked separately by the tests. Preparation is done outside these rendering measurements; it adds worker work and bounded storage. These results do not establish actual game FPS. Hybrid retains more native artwork and remains somewhat more expensive than styled mode in this fixture, even after optimization.

## Poisoned Well regression and local correction

The next live test reported roughly 100 FPS lost late in Poisoned Well and a green native shoreline beside the modern gray contour. Its log reached 2,162,041 explored fine cells, 6,863 floor quads and 1,573 wall runs. Automap CPU samples were approximately 6.3-6.5 ms. `preparedFloors=1` confirms the preparation cache was active, while `wallsReplaced=0` shows that no native walls were being replaced. The custom `PW wall` and `PW outline` labels had fallen into the conservative detail category. Native terrain was therefore clipped and submitted in addition to the modern drawing, including blank contour filler sprites.

`HybridArtwork.hpp` now recognizes those two exact labels in automap table group 46, and `ExplorationRuntime.cpp` applies that classification only in Poisoned Well, level 202, for the tested profile. The inspected local tables contain 289 such frame IDs. An unknown alias in that group or an object reference protects the frame from replacement. Uses in other areas remain independent. These are contour sprites, not the textured water artwork retained in campaign areas. Native rendering returns if replacement geometry is unavailable. This adds a fixed 64 KiB classification array, with no new per-frame allocations or growing cache.

The runtime regression tests cover both zooms, early contour suppression before clipping, once-per-pass modern drawing, exact clipping of retained icons/water/details, same-group conflicts, object protection, unrelated-area aliases, malformed/reloaded tables and native fallback. All three Win32 Release suites pass with the optional local tables and both artwork sheets. The actual-table audit checks all 289 contour IDs and unchanged roles outside level 202.

A separate synthetic panning scene uses the installed tables and sampled Poisoned Well contour IDs plus retained icon/water IDs. It contains 1,578,484 explored fine cells, 6,804 floor quads and 1,432 wall runs, with 4,615 native-cell hook calls per pass. Across three paired x86 Release runs of 180 passes, median callback CPU time fell from **3.32 to 1.83 ms (45%)** against the preceding local build. Native quad submissions fell from 1,439,100 to 141,840 across each run; forwarded contour cells fell from 459,000 to zero. Retained icon/water cell counts, modern vertex count and modern polygon area were identical. Geometry and renderer callbacks are synthetic; this measures removal of duplicate work, not in-game FPS or GPU time. The previous benchmark above used generic retained detail labels and did not expose this classification regression. Other endgame tilesets still need separate visual/performance checks.

## In-game check

Use the Test PD2 shortcut. Check Dark Temple and a map containing water in both overlay and corner modes. Confirm native details and icons, gray wall outlines, shaded floors and the red reveal edge. Walk until most of the map is explored and compare FPS while moving and standing still. Also check zoom/pan, a town gate and an adjoining campaign-area transition. The local build was approved for beta.3 publication; broader late-map FPS comparisons remain useful.

For this correction, prioritize Poisoned Well: confirm the green duplicate shoreline and thick white native walls disappear while the modern outline remains. Explore most of the map again and compare late-run FPS. Startup should report `289 Poisoned Well contour IDs`, and `HYBRID wallsReplaced` should increase during that area. Keep the log for comparison; these counters alone do not prove correct appearance or restored FPS.

To compare the appearance/performance tradeoff, set `MapsStyle=styled` and restart; the new rendering optimizations still apply. Reverting the DLL requires closing the game and restoring the recorded backup. The compatibility fingerprints and release DLL hash in `compatibility.json` describe the current beta; the optimization measurements here were collected before beta.4's radius adjustment.

The user subsequently confirmed Poisoned Well had virtually no frame drops. Dark Temple required the additional [artwork padding optimization](native-artwork-padding.md), also included in beta.3. These reports do not establish sustained FPS on other systems.
