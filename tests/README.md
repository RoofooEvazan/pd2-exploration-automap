# Exploration and rendering tests

Three test programs check the map algorithms and renderer-call behavior using synthetic data and mocked callbacks. The default suite does not need a game installation.

| Test | What it checks |
| --- | --- |
| [map_style_tests.hpp](map_style_tests.hpp) | Style cycling, saving, legacy precedence, Original bypass, retained discovery/caches and Styled navigation/terrain clipping. |
| [entrance_visibility_tests.hpp](entrance_visibility_tests.hpp) | Entrance classification, palette signatures, capped opacity/brightness, both views, unchanged clips/UVs, bounded batches and state restoration. |
| [mask_tests.cpp](mask_tests.cpp) | Explored cells, reveal boundaries, clipping coverage, movement, and session state. |
| [runtime_tests.cpp](runtime_tests.cpp) | Native clipping, hybrid contours and protected details, sewer styling, towns, shared campaign areas, session guards, and bounded native game-table reads. |
| [boundary_menu_tests.hpp](boundary_menu_tests.hpp) | Thirteen exact RGB presets, independent persistence, picker navigation/Back/Escape, menu alignment and headings, save failures, native guards, resource ownership and unchanged drawing geometry. |
| [map_marker_tests.hpp](map_marker_tests.hpp) | Shrine/event tables, guarded loaded-unit reads, sample expiry, visibility, native registration and duplicate suppression in both sizes. |
| [fixed_distance_tests.hpp](fixed_distance_tests.hpp) | Hardcoded 33-subtile coverage, ignored legacy distance settings, resolution independence, movement, teleport gaps, towns and preserved appearance settings. |
| [styled_tests.cpp](styled_tests.cpp) | Floor shading, red open edges, wall outlines, cached-versus-full geometry, and background worker snapshots. |

After [building the project](../README.md#build-from-source), run from the repository folder:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

See the [testing guide](../docs/testing.md) for optional local integration checks and diagnostic fields. Passing these tests does not validate another game's binary layout or establish long-session stability.
