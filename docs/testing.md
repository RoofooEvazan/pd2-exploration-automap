# Testing and diagnostics

Configure and build a Win32 Release using the [build instructions](../README.md#build-from-source), then run:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

The default suites use synthetic data and mocked game/renderer calls. They need no game installation, extracted assets or network access. Warnings are errors. Passing tests validates geometry and callback contracts, not undocumented addresses in another game build.

## Automated coverage

| Suite / component | Checks |
| --- | --- |
| `mask_tests` | Sparse cells, circular reveal, frontier extraction, independent raster coverage, movement and session masks. |
| `styled_tests` | Floor connectivity, exact shade coverage, open-edge classification, viewport clipping, incremental/full-build equivalence, prepared coordinates, wall joins, worker coalescing and session isolation. |
| `runtime_tests` | Native quad coverage and UVs, fractional lines, state restoration, failure cleanup, terrain classification, town/exploration unions, shared campaign layers, bounded table reads and fallback paths. |
| `map_style_tests.hpp` | Original bypass, Styled terrain suppression and navigation retention, both zooms, style cycling, persistence, legacy precedence, failed saves and retained history/caches. |
| `boundary_menu_tests.hpp` | Seven RGB presets, independent color/thickness persistence, stationary thickness updates, Back/Escape, label alignment, heading fonts, resource ownership and menu signature guards. |
| `appearance_defaults_tests.hpp` | Seven-color defaults, independent map styles and ignored legacy opacity settings. |
| `native_fade_tests.hpp` | Native Fade modes, center-band coverage without overlaps, panel offsets, Auto animation, guarded reads, minimap bypass and wall/water alpha restoration. |
| `area_entry_tests.hpp` | Neighbor capture, retained contours, unfinished-terrain fallback and layer isolation. |
| `water_tint_tests.hpp` / `ground_bank_tests.hpp` | Cached/fresh shoreline splits, native river traces, layered DT1 water material, dry-prop and bridge exclusions, bank tags and cache ownership. |
| `native_water_tests.hpp` | No added green water fill in any style, preserved shoreline coverage, white-only Original fullscreen dimming and palette restoration. |
| `fixed_distance_tests.hpp` | Exact 33-subtile circle, ignored legacy radius inputs, resolution independence, quarter-cell movement, teleport gaps and town exemptions. |
| `entrance_visibility_tests.hpp` | Capped alpha/RGB gain, classification, both views, exact-once drawing, copied contexts/clips, queue limits, palette binding, grouped uploads and restoration after exceptions. |
| `map_marker_tests.hpp` | Shrine/event definitions, loaded-unit snapshots, pointer/cycle guards, expiry, explored-location gating, native registration and duplicate suppression. |

The header-based checks run within `runtime_tests`; there are three test executables. The [test source index](../tests/) links to each file.

Sewer regressions cover shared wall endpoints, full-width clipping, native fallback, owned profile caches, scoped water classification, duplicate tiles, shared-edge removal and bridge exclusion. Poisoned Well checks ensure contour suppression preserves icons/water and unrelated-area aliases. Padding checks compare visible pixels and UVs with the full-canvas reference.

Wall-stroke regressions check binary16 position rounding, both zooms, panning, reversed slopes, cache bounds and single-wall invalidation against fresh geometry. Boundary tests compare interval reachability with a pixel-based reference, including thin solid walls and open entrances.

Cache regressions compare cold/cached coverage across holes, pan, both zooms, growth, shrink, town unions, collisions and complex cells. All cache callers share invalidation state. Prepared-floor tests retain projection/order and fallback at capacity limits. Session tests cover callback gaps, transient state reads, menu epochs, repeated seeds, act changes and stale native layers during travel.

## Optional fixtures

These checks use private files from the development installation. Fixtures are not required to build or use the plugin and are not distributed. Unset variables skip the corresponding check; supplied fixtures must match the expected schema or capture.

| Variable | Input |
| --- | --- |
| `PD2_AUTOMAP_ARTWORK_DIR` | Directory containing `MaxiMap.dc6` and `MaxiMapS.dc6`. Decodes both sheets, checks occupied bounds and validates sewer profiles against native pixels. |
| `PD2_HYBRID_TABLE_DIR` | Directory containing `automap.txt`, `Objects.txt`, `Levels.txt`, `MonStats.txt` and `MonStats2.txt` from the development setup. Audits known artwork IDs, layers and marker definitions. |
| `PD2_ARCHIVE_TABLE_DIR` | Private archive snapshots of `Levels.txt`, `automap.txt` and `Objects.txt`, read through the mocked file-reader interface. |
| `PD2_FLOOR_PROBE` | Original development collision capture. This legacy test uses fixed seed coordinates; arbitrary room dumps are not substitutes. |

Example:

```powershell
$env:PD2_AUTOMAP_ARTWORK_DIR = 'D:\LocalFixtures\AUTOMAP'
try { ctest --test-dir build -C Release --output-on-failure }
finally { Remove-Item Env:\PD2_AUTOMAP_ARTWORK_DIR }
```

The inspected direct tables contain 132 campaign entries, 111 ordinary wall IDs and 289 scoped Poisoned Well contour IDs. The two artwork sheets each contain 1,974 frames. Fixture counts are not runtime requirements for other supported table sets.

## Runtime logs

`ExplorationMask.log` is written beside the game executable. `Dormant` means the activation flag is absent; `Not installed` indicates a missing module or unsupported hook chain.

| Field | Meaning |
| --- | --- |
| `GROUND_BANKS` | Per-room bank/water material reads, matching tiles, tagged cells and read failures. |
| `TABLE` | Read status, byte count and resolver source. A successful read still needs to pass parsing. |
| `CAMPAIGN layers`, `HYBRID classified`, `STYLE` | Accepted definitions and selected internal styles. Styles use the same names as the menu: original, native, hybrid and styled. |
| `BOUNDARY`, `APPEARANCE` | Menu installation and loaded/saved styles, colors and thickness. |
| `FADE native fullscreen controls ready` | Native Fade binding accepted; unsupported layouts retain base custom alpha. |
| `DISCOVERY mode=hardcoded`, `radiusSubtiles=33.00` | Fixed reveal policy. |
| `mapMs` | CPU time between automap callbacks, not total frame time or GPU cost. |
| `workerBuildMs`, `revealLatencyMs` | Latest completed build duration and queue-to-completion time. |
| `rebuiltChunks` / `totalChunks` | Rebuilt versus retained geometry regions. |
| `DRAW style`, `floorQuads`, `wallRuns`, `preparedFloors` | Drawing mode, geometry counts and prepared-projection status. |
| `clipHits` / `clipMisses` | Reused versus rebuilt clipping queries. |
| `Area detected`, `layer`, `mapKey`, `layerWaits` | Layer sharing and transition deferrals. Adjoining campaign areas on the same layer should share a key. |
| `townNativeCells`, `townPreviewPasses`, `previewLevel`, `townClipActive` | Town clipping and nearby outdoor preparation. |
| `HYBRID wallsReplaced`, `detailsRetained`, `waterRetained` | Cumulative classification counts, including off-screen/unrevealed artwork. |
| `sewerTraced`, `sewerFallbacks`, `sewerWaterCells` | Accepted sewer wall/water cells and native fallbacks. Water is collected before exploration clipping. |
| `ARTWORK trimmed`, `blankSkipped`, `boundsHits`, `boundsDecoded`, `boundsFailures` | Occupied-bounds reuse, decoding and fallback. |
| `ENTRANCES drawn`, `palettePasses`, `fallbacks` | Entrance submissions, grouped brightness passes and native fallback counts. |
| `MARKERS sampled`, `added`, `alreadyNative` | Loaded-unit sampling, fallback icons and native duplicate skips. |

Counters are cumulative unless stated otherwise. Periodic samples can repeat the latest worker result while standing still; they are not separate builds. Cache/classification counters do not measure FPS.

## Gameplay checks

Use the [documented offline launch](../README.md#install-and-run), retaining custom direct-file flags. Close the game before replacing the DLL and preserve the INI.

1. Cycle Original, Native, Hybrid and Styled independently under Maps Styling and Campaign Styling, in both views. Check native artwork/discovery and dimmed white fullscreen walls in Original, original artwork with a boundary in Native, native details in Hybrid, and contour terrain with navigation artwork in Styled. Return to Hybrid and verify retained exploration and colors.
2. Open all three color lists in each group. Check mouse/keyboard selection, headings, Cyan, White and the seven-color palette, Back/Escape and saved preferences after restarting. Cycle thickness through 0.5, 1.0, 1.5 and 2.0 while stationary; only the selected group should change.
3. Cycle Fade through No, Center, Everything and Auto in Original, Native, Hybrid and Styled. Check fullscreen walls and water, center bands while panning/opening side panels, and Auto while standing, walking and running. Confirm the corner minimap is unchanged.
4. Visit a road, waypoint, shrine/event, cave entrance and stair/exit symbol. Check the reveal edge and entrance emphasis in both views.
5. Cross a town gate and an adjoining campaign boundary, then return. Approach Black Marsh/Tamoe Highland slowly: walls inside the reveal circle should appear before the area label changes. Check the walls and reveal edge immediately on entry, without walking farther. Repeat in Hybrid and Styled, in both views. Repeat portal/waypoint travel and hide the map for several seconds. Save/exit and start a new game to check reset behavior.
6. Walk continuously while rooms load and watch already-drawn walls for blinking, in Hybrid and Styled at both map sizes. Inspect sewer walls, channels and bridges, then a nearly explored endgame map. Compare moving/stationary frame times and capture the log.

Check Act 1 raised grassy banks separately from stone walls and the river beside town. In Poisoned Well, inspect partial shore tiles, layered floors, dry props and bridges in Hybrid; verify that no extra green water fill is submitted. Test different Water Color choices independently from Wall Color.

Historical offline checks confirmed entry walls, stable contours while walking, Act 3 blue banks, native campaign details, the Harrogath gate, adjoining-area persistence, sewer wall/water appearance, archive/direct loading, shrine/event icons and color menus. Full campaign coverage, controlled entrance/style performance comparisons and long-session stability remain under test. See [performance](performance.md) for the scope of existing measurements.
