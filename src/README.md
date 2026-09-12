# Exploration and rendering source

The C++17 implementation of the plugin: tracking explored space, preparing map geometry, and drawing it through the tested PD2/D2GL renderer.

| File | Purpose |
| --- | --- |
| [ExplorationRuntime.cpp](ExplorationRuntime.cpp) | Connects the DLL to the game, reads player state, enforces the compiled reveal radius, handles towns, and submits automap drawing. |
| [GameTables.hpp](GameTables.hpp) | Loads owned, size-limited table data through the verified Storm file resolver, using archives or active direct overrides. |
| [ExplorationMask.hpp](ExplorationMask.hpp) | Tracks explored cells and grows the reveal area as the player moves. |
| [BoundaryColors.hpp](BoundaryColors.hpp) | Defines five frontier colors while preserving the original red shades and opacity. |
| [BoundaryMenu.hpp](BoundaryMenu.hpp) | Adds a guarded color row to the supported native Automap Options menu. |
| [SessionIdentity.hpp](SessionIdentity.hpp) | Distinguishes game sessions from loading, map pauses, and travel using menu/player/act-seed identity. |
| [ProjectedMask.hpp](ProjectedMask.hpp) | Converts the exploration mask into screen-space clipping for native artwork. |
| [RasterClipCache.hpp](RasterClipCache.hpp) | Reuses exact artwork clipping across frames and map panning, with bounded storage and immediate coverage invalidation. |
| [HybridArtwork.hpp](HybridArtwork.hpp) | Identifies replaceable walls and area-specific Poisoned Well contours from active game tables while protecting textured water, landmarks, and object icons. |
| [ArtworkBounds.hpp](ArtworkBounds.hpp) | Caches occupied sprite bounds so retained native artwork avoids clipping transparent padding without changing its visible pixels. |
| [CampaignLayers.hpp](CampaignLayers.hpp) | Reads expected campaign layers so adjoining areas share explored maps and stale transition callbacks cannot mix separate layers. |
| [NativeWallTrace.hpp](NativeWallTrace.hpp) | Builds straight sewer wall profiles with shared endpoints, checking them against native artwork before replacement. |
| [SewerWater.hpp](SewerWater.hpp) | Joins sewer water tiles into channel outlines and reuses unchanged perimeters without rebuilding internal seams each frame. |
| [FrontierContacts.hpp](FrontierContacts.hpp) | Finds where the reveal edge touches visible artwork in fallback mode. |
| [NativeFloorReader.hpp](NativeFloorReader.hpp) | Reads loaded room collision data for floor and wall geometry. |
| [StyledMap.hpp](StyledMap.hpp) | Builds shaded floors, gray wall outlines, and red unexplored edges. |
| [StyledChunks.hpp](StyledChunks.hpp) | Reuses unchanged map regions and rebuilds regions affected by exploration. |
| [StyledProjection.hpp](StyledProjection.hpp) | Clips floor shapes and wall lines, and constructs fractional-width contour quads for hybrid styling. |
| [PreparedFloors.hpp](PreparedFloors.hpp) | Prepares bounded floor projection and bounds on the worker so each draw only pans and clips them. |
| [StyledWorker.hpp](StyledWorker.hpp) | Builds geometry on one background worker using owned snapshots of the map data. |

Start with the [architecture guide](../docs/architecture.md) to follow the drawing path. The [build instructions](../README.md#build-from-source) produce the 32-bit Windows DLL. Engine addresses and data layouts are specific to the tested binary set.
