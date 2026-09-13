# Building and testing

Use this guide to understand each test suite, enable optional local integration checks, and read the plugin's runtime diagnostics.

The [README](../README.md#build-from-source) gives the CMake commands. All three default tests run without a game installation, external fixture, or network access. Configure a Release **Win32** build with MSVC and run CTest.

| Test | Main checks |
| --- | --- |
| `mask_tests` | Independent explored cells, movement, frontier extraction, clipping and session behavior |
| `runtime_tests` | Mocked native/Glide callbacks, quad coverage and UV interpolation, contact decoding, fractional lines, styled color restoration, exception cleanup, five town exemptions, town footprints, outdoor previews and gate-transition persistence |
| `styled_tests` | Reference erosion, exact shade coverage, viewport clipping, red open edges versus gray wall edges, collision connectivity, chunk/full equivalence, resets, teleports, worker coalescing and session isolation |

Warnings are errors. Tests check actual geometric coverage and renderer-call contracts rather than loading a DLL into the game. Their success does not validate undocumented engine addresses or long-session behavior.

Campaign tests additionally cover native artwork IDs and exact clipping at both zooms, native town/exploration unions, a single shading pass without duplicate wall lines, independent style settings, original-mode bypass, long callback gaps, transient reads, cross-act travel, menu epochs, same-seed new games, and player/seed changes. See [campaign-prototype.md](campaign-prototype.md) for live checks still required.

The v0.1.1 regression checks cover town rectangle rasterization at both zoom levels, tile-to-subtile validation, outdoor preparation before crossing, retained exploration after exit and reentry, exact native town coverage, duplicate-free fallback clipping, once-per-pass mixed rendering, off-screen rejection, and act/layer isolation. All three Win32 Release suites passed. A live Harrogath/Bloody Foothills test confirmed consistent outdoor styling when approaching, crossing, and returning through the gate. Other town entrances have not received the same visual validation.

## Optional local artwork check

If you have legally obtained and already extracted `MaxiMap.dc6` and `MaxiMapS.dc6`, point `PD2_AUTOMAP_ARTWORK_DIR` at their directory. `runtime_tests` then decodes all frames in both files in addition to the synthetic tests:

```powershell
$env:PD2_AUTOMAP_ARTWORK_DIR = 'D:\LocalFixtures\AUTOMAP'
ctest --test-dir build -C Release -R runtime_tests --output-on-failure
Remove-Item Env:\PD2_AUTOMAP_ARTWORK_DIR
```

Unset means skip that optional check. A supplied directory with missing or malformed files fails. Do not add the sheets to this repository or a release.

## Optional development collision fixture

`PD2_FLOOR_PROBE` can point to the original local endgame-map development capture. This legacy integration check uses fixed seed coordinates from that capture, so an arbitrary room dump is not a substitute. The fixture is not distributed or required; the default synthetic suite covers the same floor/geometry algorithms without game data. The test does not write preview files.

## Runtime diagnostics

### Automatic game-table loading

The default runtime suite covers the native Storm binding and owned table reads with synthetic data. An optional `PD2_ARCHIVE_TABLE_DIR` can point to private snapshots of the three TXT files from the installed PD2 archive; it tests parsing through the same reader interface. No archive library or game asset is needed for the default tests. Do not distribute these snapshots. See [game-table validation](game-tables.md#validation) for the live comparison with and without direct overrides.

On the first automap update, `TABLE` lines report each file read. `CAMPAIGN layers`, `HYBRID classified` and `STYLE` then confirm accepted definitions and the resulting styles. Missing-table fallback may occur even when the memory hooks installed successfully.

### Optional hybrid artwork-table audit

The synthetic runtime suite always checks conservative classification, protected details, and hybrid drawing. To additionally check the current development installation's campaign frame IDs against its own tables:

```powershell
$env:PD2_HYBRID_TABLE_DIR = 'C:\Program Files\Diablo II\ProjectD2\data\global\excel'
try { ctest --test-dir build -C Release --output-on-failure }
finally { Remove-Item Env:\PD2_HYBRID_TABLE_DIR }
```

This reads `automap.txt`, `Objects.txt`, and `Levels.txt` locally; these files do not belong in the repository. The inspected tables produce 111 eligible ordinary wall IDs and preserve the tested roads, water, waypoint, cave/temple entrances, stairs, cages, landmark artwork, and six Act 3 sewer wall IDs. The layer audit checks 132 campaign entries, shared jungle layers, distinct sewer layers, blank numeric defaults, conflicting records, and act/endgame isolation. An unset variable skips this optional integration audit. This is a fixture check for the development tables, not a universal table-compatibility guarantee.

### Log fields

The game folder's `ExplorationMask.log` records initialization status, hook activity, mask size, CPU timing, and geometry counts. Missing modules or an unexpected hook chain produces a `Not installed` message. An absent `-exploration-test` flag produces `Dormant`.

Useful timing fields:

- `mapMs`: CPU time between the paired automap callbacks, not GPU frame time or a whole-game frame measurement.
- `workerBuildMs`: time spent on the most recently completed background build.
- `revealLatencyMs`: time from that request being queued to its completion.
- `rebuiltChunks` / `totalChunks`: affected regions rebuilt versus retained regions.
- `townNativeCells`: native cells forwarded through town-footprint clipping.
- `townPreviewPasses`: styled outdoor passes drawn while the player is still in town.
- `previewLevel` / `townClipActive`: the remembered outdoor area and whether the native town exemption is active.
- `HYBRID classified`: the number of ordinary wall frame IDs and separately scoped Poisoned Well contour IDs eligible for replacement after object protection.
- `HYBRID wallsReplaced` / `detailsRetained` / `waterRetained`: cumulative native-cell classification counters, not per-frame timings. A retained count does not mean the primitive was within the visible explored region.
- `Area detected` includes the native `layer` and shared `mapKey`. Adjoining campaign areas on the same layer should retain the same key; different layers and endgame level instances should not.
- `sewerTraced` / `sewerFallbacks`: cumulative sewer wall cells replaced by validated straight profiles or retained natively when unavailable/budget-limited. `layerWaits` counts callbacks deferred during a native area/layer mismatch.
- `sewerWaterCells`: cumulative recognized sewer water cells accepted for channel geometry, before exploration clipping; it is not a visible-pixel or FPS count.
- `clipHits` / `clipMisses`: cumulative reused versus rebuilt clipping queries. Cache misses retain exact drawing; these counts do not measure FPS.
- `ARTWORK trimmed` / `blankSkipped`: retained native sprites with reduced padding bounds or fully transparent frames suppressed. `boundsHits`, `boundsDecoded` and `boundsFailures` track metadata reuse, successful decoding and cached decode/read failures. Unsupported frame headers fall back before this cache.
- `OVERLAY opacity`: startup setting for custom overlay alpha. Native symbols are unchanged; the tested D2GL corner target uses fixed opacity.

The optional `PD2_AUTOMAP_ARTWORK_DIR` check also tests the six sewer wall forms at both automap sizes. Sampled straight-profile points must remain within two pixels of opaque source artwork. The synthetic runtime suite checks shared wall endpoints, full-width clipping, bounded batched submissions, owned cache reuse, and native fallback; a pixel-center raster is the clipping reference. Water checks cover scoped classification despite unrelated terrain aliases, object protection, duplicate tiles, removal of shared edges, bridge exclusion, retained native water drawing, fill/border clipping, and per-pass clearing. Wall-only drawing uses at most two batches; water adds one fill batch.

Periodic log lines can repeat the most recent worker result when the player is still. Do not interpret every line as a separate rebuild. Review diagnostics before posting them and never include game saves, account data, or proprietary assets.

The local Poisoned Well correction also audits 289 custom contour IDs from table group 46, scoped to level 202. Synthetic tests retain icons, textured water and unknown details, protect object/same-group alias conflicts, leave other areas unchanged and restore native artwork when modern geometry is unavailable. Both zooms verify that replaced contours skip native clipping while the modern outline draws once. See [large-map testing](map-growth-optimization.md) for the reported regression and benchmark limits.

The [native padding candidate](native-artwork-padding.md) checks occupied bounds against the silhouette decoder on every installed frame in both artwork sheets, plus exact visible-pixel/UV coverage before and after padding removal in synthetic rendering. Cache identity, malformed inputs, blank frames, fallback and area invalidation are also covered.

The performance revision adds coverage comparisons for cold/cached queries, holes, negative pan, both zooms, growth invalidation, town unions, collisions and over-budget cells. A cross-caller regression switches wall/cell drawing between different area masks and back, checking that shared-cache coverage follows every transition. Water perimeters are compared with an independent edge-set reference across reordered/changed frames, duplicates and the tile limit. Opacity checks cover RGB preservation, line/point submission and native color restoration. Existing rendering contracts also run at 100% to verify that the optimization preserves prior output.

## Boundary settings and fixed reveal distance

The runtime suite checks twelve exact RGB presets; independent boundary/wall persistence and reload; missing/invalid wall settings defaulting to Gray; save failure; list selection and Back/Escape; native resource ownership and layout bounds; and menu signature rejection. Both floor paths retain their geometry and shading. Hybrid, styled and sewer wall/shoreline colors preserve clipped vertices, native state restoration and alpha at both zooms; native water fill and dark casings retain their own colors.

The fixed-distance checks use the same appearance-settings reader as the runtime, with temporary INI files containing missing, zero, negative, oversized and malformed legacy radius values, paired with both old reveal modes. None changes the 33-subtile circle. The complete revealed mask matches an independent circular reference at multiple logical resolutions, including invalid view dimensions. Quarter-subtile movement, the outer radius, teleport gaps, all five town exemptions and retained outdoor history are checked. Color, style and opacity settings still load normally.

`BOUNDARY` log lines report menu installation. `APPEARANCE` reports loaded/saved boundary and wall colors. The unpublished 33-subtile build reports `DISCOVERY mode=hardcoded` and `radiusSubtiles=33.00`; it no longer reads view dimensions to choose the radius. These are functional checks of the distributed client, not anti-tamper protection or proof of online compatibility. See [live validation and limits](boundary-settings.md).

## Endgame shrine and event icons

`map_marker_tests.hpp` checks active-table classification, guarded loaded-room/unit reads, malformed pointers and bounded cycles, 200 ms sample expiry, explored-location gating, primitive clipping, native registration and per-pass duplicate suppression in both map sizes and native/styled/hybrid modes. Campaign and town exclusions are checked. The optional `PD2_HYBRID_TABLE_DIR` audit also reads `MonStats.txt` and `MonStats2.txt` from the private fixture directory. No additional files are needed for the default synthetic suite. See [marker validation](map-markers.md#validation-and-gameplay-check) for the approved live test and log fields.
