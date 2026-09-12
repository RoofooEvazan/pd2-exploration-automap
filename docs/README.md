# Documentation and screenshots

Guides to the automap's design, compatibility requirements, testing, and performance, plus gameplay screenshots.

| Guide | What it explains |
| --- | --- |
| [Boundary settings](boundary-settings.md) | Five saved frontier colors, the native menu extension, and rounded native-average reveal distance. |
| [Campaign BETA guide](campaign-prototype.md) | Hybrid contours, preserved native details, settings, game-table requirements, session persistence and known limits. |
| [Hybrid performance](hybrid-performance.md) | Clipping/water reuse, benchmark scope and overlay opacity. |
| [Large-map optimization](map-growth-optimization.md) | prepared floors, joined wall runs, growth-aware clipping and hybrid endgame testing. |
| [Native artwork padding](native-artwork-padding.md) | Dark Temple optimization: avoids clipping transparent sprite margins while preserving water and details. |
| [Automatic game tables](game-tables.md) | Removes manual table extraction by reading the active game's archives and direct overrides, with bounded reads and native fallback. |
| [Architecture](architecture.md) | How exploration, floor shading, wall outlines, renderer hooks, and background updates fit together. |
| [Performance](performance.md) | Why the map uses incremental updates, measured development results, and costs that remain. |
| [Testing](testing.md) | What the tests cover, optional local fixtures, and how to interpret runtime diagnostics. |
| [Screenshots](screenshots/) | Overlay and corner-minimap views from Spider Forest, Poisoned Well and Dark Temple. |

For installation, start with the [main README](../README.md#install-and-run).
