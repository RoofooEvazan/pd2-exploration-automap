# Exploration and rendering source

The C++17 implementation of the plugin: tracking explored space, preparing map geometry, and drawing it through the tested PD2/D2GL renderer.

| File | Purpose |
| --- | --- |
| [ExplorationRuntime.cpp](ExplorationRuntime.cpp) | Connects the DLL to the game, reads player state, handles towns, and submits automap drawing. |
| [ExplorationMask.hpp](ExplorationMask.hpp) | Tracks explored cells and grows the reveal area as the player moves. |
| [ProjectedMask.hpp](ProjectedMask.hpp) | Converts the exploration mask into screen-space clipping for native artwork. |
| [FrontierContacts.hpp](FrontierContacts.hpp) | Finds where the reveal edge touches visible artwork in fallback mode. |
| [NativeFloorReader.hpp](NativeFloorReader.hpp) | Reads loaded room collision data for floor and wall geometry. |
| [StyledMap.hpp](StyledMap.hpp) | Builds shaded floors, gray wall outlines, and red unexplored edges. |
| [StyledChunks.hpp](StyledChunks.hpp) | Reuses unchanged map regions and rebuilds regions affected by exploration. |
| [StyledProjection.hpp](StyledProjection.hpp) | Clips floor shapes and wall lines to the automap viewport. |
| [StyledWorker.hpp](StyledWorker.hpp) | Builds geometry on one background worker using owned snapshots of the map data. |

Start with the [architecture guide](../docs/architecture.md) to follow the drawing path. The [build instructions](../README.md#build-from-source) produce the 32-bit Windows DLL. Engine addresses and data layouts are specific to the tested binary set.
