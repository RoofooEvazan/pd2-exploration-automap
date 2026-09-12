# Exploration and rendering tests

Three test programs check the map algorithms and renderer-call behavior using synthetic data and mocked callbacks. The default suite does not need a game installation.

| Test | What it checks |
| --- | --- |
| [mask_tests.cpp](mask_tests.cpp) | Explored cells, reveal boundaries, clipping coverage, movement, and session state. |
| [runtime_tests.cpp](runtime_tests.cpp) | Native clipping, hybrid contours and protected details, sewer outline/shading behavior, towns, shared campaign areas, session/transition guards, and bounded native game-table reads. |
| [styled_tests.cpp](styled_tests.cpp) | Floor shading, red open edges, wall outlines, cached-versus-full geometry, and background worker snapshots. |

After [building the project](../README.md#build-from-source), run from the repository folder:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

See the [testing guide](../docs/testing.md) for optional local integration checks and diagnostic fields. Passing these tests does not validate another game's binary layout or establish long-session stability.
