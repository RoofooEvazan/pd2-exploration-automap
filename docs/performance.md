# Performance notes

How region caching keeps the reveal edge responsive as exploration grows, with measured development results and the work that still scales with map size.

The original styled implementation rebuilt all explored floor geometry after movement. Around 1.46 million explored quarter-subtile cells, a development trace recorded approximately 276-316 ms per build. Background processing kept gameplay responsive, but the reveal edge updated only a few times per second.

The current implementation caches unchanged regions, incrementally connects newly loaded floor runs, compacts adjacent quads, and skips unnecessary viewport clipping. It keeps the existing single worker and publishes completed snapshots without waiting in the game render callback.

## Development observations

The final build was checked in Dark Temple beyond 1.69 million explored fine cells. In 25 logged samples at or above 1.4 million cells:

| Measurement | Average | Range |
| --- | --- | --- |
| Latest completed background build | 11.43 ms | 4.58-20.45 ms |
| Game-thread automap callback CPU time | 1.45 ms | 1.12-1.61 ms |

The live game display showed 240 FPS with shaded floors, gray walls, and red open edges. These observations come from one development setup; they are not hardware-normalized benchmarks. Log samples report the latest completed worker result and can repeat a result. Callback CPU time does not measure GPU work or all frame costs. The source release's synthetic tests do not reproduce the complete game session.

A separate synthetic comparison with 1.5 million explored fine cells measured 24 small movements: the preceding full rebuild averaged 35.23 ms and the cached update averaged 6.46 ms. An average of 9.75 of 391 regions were rebuilt. An unchanged snapshot rebuilt none and took about 0.48 ms to assemble. After compaction, the cached drawing used 1,612 quads versus 1,606 before region partitioning. These figures describe algorithm costs in that fixture, not in-game FPS.

## Costs that remain

Snapshot comparison, drawing assembly, projection, and rendering still scale with retained geometry. Initial population, a large newly connected area, or an evicted worker area can require more work. Per-area resource limits can stop additional collision capture, while total session memory is not globally capped. The renderer continues displaying the last completed geometry during a build.

Long-session crash qualification remains outstanding. An earlier prototype triggered a D2Glide texture-cache assertion whose cause was not confirmed. The later clipping route prepares native artwork once per cell, but no claim is made that this resolves that earlier engine failure.
