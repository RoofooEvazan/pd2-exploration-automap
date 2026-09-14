# Exploration and rendering tests

Three test programs check the map algorithms and renderer-call behavior using synthetic data and mocked callbacks. The default suite does not need a game installation.

| Test | What it checks |
| --- | --- |
| [area_entry_tests.hpp](area_entry_tests.hpp) | Neighboring wall capture before zone entry, unchanged contours during pending or failed capture, tall-sprite fallback at explored ground placements, coverage-cache invalidation, return trips, fixed discovery, and layer/act/town/endgame isolation. |
| [ground_bank_tests.hpp](ground_bank_tests.hpp) | 1.13c tile-name and floor-collision reads, reversed DT1 rows, water material under decorative overlays, dry-ground prop exclusions, owned worker data and Hybrid coloring at both zooms. |
| [native_water_tests.hpp](native_water_tests.hpp) | No added green water fill, preserved shoreline coverage and bridge exclusions across all styles, white-only fullscreen wall dimming, bounded batches and palette restoration. |
| [water_tint_tests.hpp](water_tint_tests.hpp) | Water/riverbank identification, partial-water coverage and downsampled shore corners, dry-ground and bridge exclusions, contour color splits, native shoreline tracing, duplicate suppression and cached drawing. |
| [map_style_tests.hpp](map_style_tests.hpp) | Style cycling, saving, legacy precedence, Original bypass, retained discovery/caches and Styled navigation/terrain clipping. |
| [entrance_visibility_tests.hpp](entrance_visibility_tests.hpp) | Entrance classification, palette signatures, capped opacity/brightness, both views, unchanged clips/UVs, bounded batches and state restoration. |
| [mask_tests.cpp](mask_tests.cpp) | Explored cells, reveal boundaries, clipping coverage, movement, and session state. |
| [runtime_tests.cpp](runtime_tests.cpp) | Native clipping, hybrid contours and protected details, sewer styling, towns, shared campaign areas, session guards, and bounded native game-table reads. |
| [boundary_menu_tests.hpp](boundary_menu_tests.hpp) | Thirteen exact RGB presets and Light Blue water, independent color/thickness persistence, stationary thickness changes, picker navigation/Back/Escape, menu alignment and headings, save failures, native guards, resource ownership and unchanged drawing geometry. |
| [map_marker_tests.hpp](map_marker_tests.hpp) | Shrine/event tables, guarded loaded-unit reads, sample expiry, visibility, native registration and duplicate suppression in both sizes. |
| [fixed_distance_tests.hpp](fixed_distance_tests.hpp) | Hardcoded 33-subtile coverage, ignored legacy distance settings, resolution independence, movement, teleport gaps, towns and preserved appearance settings. |
| [styled_tests.cpp](styled_tests.cpp) | Floor shading, stable binary16 wall widths and stroke-cache equivalence, open edges, boundaries behind thin walls and at open entrances, interval connectivity against pixel BFS, cached-versus-full geometry, and background worker snapshots. |

After [building the project](../README.md#build-from-source), run from the repository folder:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

See the [testing guide](../docs/testing.md) for optional local integration checks and diagnostic fields. Passing these tests does not validate another game's binary layout or establish long-session stability.
