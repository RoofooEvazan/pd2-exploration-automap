# How the automap works

Runtime hooks, data ownership and cache limits for the tested binary set. The [source index](../src/) describes each component.

## Rendering flow

```text
automap begin
  -> read player/session state; update exploration history
  -> Original: leave game rendering unchanged
  -> Native: clip original artwork and draw the exploration boundary
  -> Native/Hybrid/Styled: capture loaded collision data; submit worker snapshot

native terrain cell
  -> draw completed floor/contour geometry once per pass
  -> classify native artwork for replacement or retention
  -> clip retained textured primitives before draw
  -> queue eligible entrances and Hybrid sewer outlines

automap end
  -> submit queued sewer geometry and entrance artwork
  -> supply missing shrine/event icons from loaded-unit snapshot
  -> draw completed geometry even when no native tiles arrived
  -> draw a clipped entry boundary during worker warmup
  -> restore per-pass state and record timing
```

Exploration uses a sparse grid with 0.25-subtile cells. Player coordinates come from the path's unsigned 16.16 world positions. Movement reveals a hardcoded disk of 132 fine cells (33 world subtiles). Display resolution and old distance settings cannot change it. See [boundary settings](boundary-settings.md) for the compiled policy. The mask belongs to a tracked game session and area/seed key; it is independent of native automap sprite IDs and wall layers. This remains a distance mask, not exact native tile exploration or visibility through doors and walls.

Town IDs `1`, `40`, `75`, `103`, and `109` retain native artwork within their level rectangle. The exemption follows the terrain footprint instead of applying to every native cell while the player is in town. Nearby outdoor terrain can therefore use the styled map before the player's area ID changes.

Near a town exit, the game thread samples already-loaded outdoor floor grids every 250 ms. The nearest walkable outdoor cell supplies the connectivity seed, while exploration still grows around the actual player position. This preview selects the same seeded outdoor area mask and geometry cache used after crossing the gate, preserving exploration when walking out and back. Only one neighboring area is previewed at a time; no room loading or native reveal calls are added.

Town bounds come from guarded level reads, are converted from DRLG tiles to world subtiles, and must enclose the current room's collision grid. The cached footprint is scoped to the session, act, seed, and automap layer. A projected rectangle clips native town pixels; fallback drawing unions that coverage with explored coverage without drawing overlapping pixels twice. An off-screen town is rejected before inspecting native frame data. While the outdoor result is being prepared, explored outdoor terrain retains its native artwork.

## Styled map

The render thread reads only already-loaded room collision grids, copying them into owned immutable room records. Static wall (`0x0001`) and blank (`0x0020`) flags exclude cells from floor shading. Transient actor/item/object flags do not turn floor into holes. Connectivity selects components reached by the player, limiting isolated collision artifacts.

Act 1 outdoor room captures also read ground tile library names from the supported 1.13c layout: the floor list is at `room+8`, and D2CMP stores the filename pointer at `tileEntry+0x58`. A bank-material mask follows the owned collision snapshot; the worker uses it to split contour colors without adding geometry or discovery. Each room scan is capped at 16,384 floor tiles. The library lookup still needs live confirmation on raised grassy banks.

The worker combines connected floor with the explored region. Seven shade layers form the reveal band. Open floor beyond the mask seeds the colored frontier; a boundary against known walls does not. Colors are selected at draw time. Wall strokes use the configured core RGB and a dark casing; source brightness also matters in D2GL's fixed-opacity minimap. The edge uses layered geometry, not a continuous blur.

`StyledChunks.hpp` caches 64-by-64 fine-cell regions (16-by-16 world subtiles). Snapshot differences invalidate affected regions with filter padding sized to the boundary width (14 fine cells at the default, up to 26). Half-open ownership prevents repeated shade and wall coverage at cache boundaries. Adjacent matching quads are compacted before publication. Projection quickly accepts contained quads, rejects off-screen quads, and clips only viewport crossings.

One background worker owns the derived floor/geometry cache. It has one replaceable pending request and one completed result. Requests contain owned data and shared immutable room copies, never borrowed game pointers. Results include session and area identifiers. The render thread uses the last completed drawing while work continues. Submission is limited to once per 40 ms; room capture is sampled at 250 ms intervals and immediately on area entry.

Campaign capture includes already-loaded neighboring rooms on the same verified act/layer, matching the shared exploration mask. It does not wait for the player's area label to change. Town rooms and rooms on different layers stay separate; endgame capture remains per level. Capture does not reveal cells or load additional rooms.

The completed drawing records its room count and immutable collision coverage at both map sizes. Pending or failed room capture leaves the completed contours visible. Only native pixels outside that completed coverage fill unfinished terrain, still clipped to exploration. A one-subtile rim allows native fallback at unknown room edges; eroding the union of known rooms prevents seams between captured rooms. This avoids switching the whole map's walls during walking.

Coverage is prepared on the worker only when its room set changes and is shared with completed results. Movement-only rebuilds reuse it. A separate bounded clip cache handles unfinished terrain; a new coverage snapshot invalidates its full and partial entries. Settled frames retain the early native-wall suppression path.

## Native artwork fallback

When styled output is unavailable, the original cell preparation runs once. The final axis-aligned textured quad is clipped into disjoint visible strips before the original draw call; position and texture coordinates are interpolated together. This avoids repeatedly asking the game to prepare or crop its texture-cache entry.

Fallback frontier accents appear only where the frontier intersects opaque artwork, with a small extension at either end. DC6 silhouette data is copied into a bounded owned cache. Game texture-cache allocation or eviction metadata is never modified. Fractional lines are submitted through the existing renderer path after substituting the final vertex coordinates.

## Tested interception points

These are module-relative offsets for the binary hashes in [compatibility.json](../compatibility.json), not portable addresses.

| Module + offset | Purpose |
| --- | --- |
| `D2Client.dll + 0x6269E` | Automap begin call |
| `D2Client.dll + 0xC3AA1` | Automap end call |
| `D2Client.dll + 0x604EA` | Native automap terrain cell call |
| `D2Glide.dll + 0xA33F` | Prepared textured quad submission |
| `D2Glide.dll + 0x94BA` | Floating-point line submission |
| `D2Glide.dll + 0x944C` | Floating-point point submission |

Styled drawing also depends on `glide3x.dll` exports `_grDrawVertexArray@12` at `+0xB4020` and `_grConstantColorValue@4` at `+0xB4210`. The x86 vertex ABI is 28 bytes. `D2gfx.dll` ordinal 10010 supplies the native line path. Begin/end chaining tolerates the inspected D2GL wrapper hooks and preserves their targets.

The initializer checks selected call opcodes and targets, prepares all affected pages before editing any calls, and changes process memory only. Other offsets and layouts in the readers are equally build-specific. Do not treat these narrow signature guards as complete executable validation.

The runtime uses `GameTables.hpp` to resolve these tables through native Storm after PD2 archives are mounted. It honors active direct overrides and performs no recurring table reads. See [automatic table loading](game-tables.md) for the additional source binding profile and validation.

Endgame shrine/event icons use an additional bounded snapshot of loaded units every 200 ms. Missing, unregistered icons are submitted through the existing clipped native cell path before the end callback chains to the renderer. Native registration and per-pass draw checks prevent duplicate icons. No new hook, room loading, preset traversal or game-state write is added. See [marker behavior and limits](map-markers.md).

## Lifetime and resource limits

The campaign hybrid mode reuses the styled floor/contour cache. `HybridArtwork.hpp` conservatively classifies native frame IDs from the active game's automap and object tables on the first valid automap update. Ordinary walls use `drawHybridWalls`' selected core color and dark casing, while native navigation details, icons, and town portions remain. Hybrid sewer levels 92/93 skip floor-contour walls: `cellHook` instead queues straight isometric profiles from `NativeWallTrace.hpp`, validated against eligible native wall silhouettes. Unsupported or budget-limited walls retain their native sprite. `SewerWater.hpp` collects recognized drain/water diamonds and cancels shared edges; bridges are excluded. Native water stays visible beneath a faint fill and channel border. `endPass` groups fill, casing, wall cores and independently colored water cores into batches capped at 65,536 vertices each, clipped to explored pixels and the viewport. The wall cache holds at most 24 owned profiles and water collection at most 8,192 tiles per pass; no native texture memory is modified. Missing required artwork tables select clipped-native fallback for Hybrid and Styled. `CampaignLayers.hpp` reads expected layers from active `Levels.txt`; a transition callback with a mismatched native layer cannot mutate shared exploration or capture geometry. Missing layer definitions preserve per-level cache keys. See [campaign-prototype.md](campaign-prototype.md) for the exact styling and limits.

The worker's derived cache retains up to 32 entries. Campaign areas on the same native automap layer share an entry, keyed by seed, act, and layer; endgame areas remain keyed by seed and level. The shared mask, retained room copies, and completed geometry survive crossing an adjoining campaign area's boundary. Each cache entry's room-copy capture accepts up to 2,048 rooms and 2,000,000 collision cells, with room dimensions capped at 512 per axis. The fallback silhouette cache clears around 4 MiB or 4,096 entries. These limits bound particular caches, not the complete process memory footprint: session exploration masks and render-thread area data can still accumulate until reset. A capture limit can leave later terrain unavailable for styled output.

The session resets on a confirmed menu return, player identity change, or a changed seed in a previously visited act. It preserves exploration through callback gaps and temporary room/path unavailability. The watcher samples the D2Win control-list pointer at RVA `0x214A0` and the player-unit pointer every 50 ms, publishing only an atomic menu epoch; the draw thread owns mask/cache changes. The binding validates the image and relocated references; failure to bind or start the watcher prevents activation. Nothing is persisted across process restarts. The worker lives for the process lifetime; live DLL unloading is unsupported.

## Porting checklist

For another build, verify the player/path/room/level structures, room collision buffers, projection globals, every call site's ABI and target, wrapper export layout, and rendering state behavior. Run the synthetic tests, validate installation guards against the new binary set, then test offline across movement, area transitions, town bypass, minimap modes, large maps, and long sessions. Publish a separate compatibility profile rather than weakening the existing checks.

Rendering uses a bounded cache of exact clipping rectangles in stable automap coordinates and reuses unchanged sewer-water perimeters. Custom full-screen alpha is configurable independently of the tested D2GL corner-map capture. See [performance.md](performance.md) for invalidation, bounds, validation and benchmark scope.
