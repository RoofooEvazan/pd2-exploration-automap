# Entrance and exit visibility

Local, unpublished follow-up to beta.6. Recognized native entrance/exit symbols and cave/stairway artwork are easier to see in both Hybrid and Styled campaign/endgame automaps. It changes the appearance of existing artwork only: no exit arrows, quest markers, added geometry or extra discovery.

## How the boost works

- Fullscreen artwork using a translucent native draw mode receives 1.75 times its existing alpha, rounded and capped at 255. Native alpha values 64, 128 and 192 become 112, 224 and 255.
- Already-solid fullscreen artwork receives a 1.75 RGB brightness multiplier, with each channel capped at 255; its opacity cannot increase further.
- D2GL forces corner-minimap artwork to 0.9 alpha, so the corner view uses the RGB brightness multiplier. This is a visibility adjustment, not a change to D2GL's fixed minimap alpha.

Existing shapes, texture coordinates, native viewport cropping and exploration clips are retained. Black stays black; channels already at maximum cannot brighten further. Hue may shift slightly when individual channels reach the cap. Wall/boundary colors, water, roads, waypoints and shrine/event artwork keep their existing rendering.

## Classification and rendering

`HybridArtwork.hpp` reads entrance labels from the active automap and object tables independently of its geometry roles. It recognizes named exits, cave entrances, stairs, trapdoors and selected native cave/crypt entrance labels. Conflicting or unknown aliases retain ordinary rendering. The supported native generic stair symbol (frame 308, absent from the tables) is eligible only when no active table alias overrides it. Decorative Arcane and explicitly fake stairs are excluded.

Eligible visible cells are collected during the native automap pass and drawn once after floor/wall batches, before the plugin's missing shrine/event fallback. Contexts and clip rectangles are copied; artwork pointers are borrowed only within that pass. Native preparation and texture caching still run once per cell. Final-quad alpha is restored after drawing.

For brightness, one temporary palette covers the entire entrance group. The validated D2GL palette export flushes preceding vertices before changing palette state; the original palette is restored immediately after the group. The game's palette array is never modified. This adds at most two palette uploads per automap pass, not per entrance, with no extra sprite draws or texture copies. The queue accepts at most 64 cells, 512 clips per cell and 8,192 total clips per pass. Retained clip capacity is bounded to 512 KiB. Overflow keeps the original immediate native drawing.

The optional palette binding checks the supported D2Glide image, relocated palette references, call/import chain and D2GL export address. Binding failure leaves native entrances unchanged. Unknown view states and failed palette reads also retain native drawing. The boost applies to Hybrid and Styled; Map Style Native uses untouched game rendering.

## Validation and local run

Build the Win32 Release DLL and run all three suites using the [build instructions](../README.md#build-from-source). Tests exercise +75% alpha and RGB limits, both views, exact-once clipping and UVs, copied context/clip lifetime, bounded queues, grouped uploads, palette/alpha restoration on exceptions, signature rejection, unknown-state fallback, non-entrance preservation and hidden entrances. Optional installed-table fixtures cover native cave/stair symbols and alias exclusions.

Close the game before replacing its plugin. Preserve `ExplorationMask.ini`, then use the normal offline Test PD2 launch: `Game.exe -3dfx -direct -exploration-test -log` from its ProjectD2 directory. Check an explored cave entrance and a small colored stair/exit icon in fullscreen and corner views, including at the reveal edge. Compare FPS while moving past several entrances. The log reports `Entrance visibility ready` at startup and cumulative `ENTRANCES drawn`, `palettePasses` and `fallbacks` counters.

Automated tests pass with local artwork/table/archive fixtures. Live appearance and frame-time comparison are pending. Recognition follows the tested artwork/tables; custom aliases may intentionally retain native brightness. This does not change the compiled 33-subtile discovery radius or establish online compatibility.
