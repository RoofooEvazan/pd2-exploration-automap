# Native artwork padding optimization

Included in v0.2.0-beta.3, following the Poisoned Well correction. The combined local build was approved for publication; a controlled long-run FPS comparison is still pending.

## Observed result and remaining problem

The user confirmed Poisoned Well now has virtually no frame drops. In the same session, the endgame test map still lost approximately 100 FPS late in exploration. Its log reached 2,803,777 explored fine cells, 14,331 floor quads and 2,913 wall runs, with prepared floor projection active. The final ten automap CPU samples averaged about 5.07 ms. Unlike Poisoned Well before its correction, ordinary walls were being replaced: the remaining workload included roughly 1,617 water and 473 detail classifications per pass, producing about 5,206 native clipped quads per pass near the end. Classification counters include off-screen and unrevealed artwork.

The endgame test map uses small water/detail patterns with large transparent margins in their native sprite canvases. For example, the inspected large-map frame 266 has a 16-by-32 canvas but its opaque pixels occupy only a 10-by-8 rectangle at the bottom. The smaller artwork sheet has the same kind of padding. The optimization targets this empty space without removing the water texture or guessing which decorative walls should disappear.

## Change

`ArtworkBounds.hpp` validates the RLE frame data and calculates the occupied rectangle with one texel of padding for bilinear sampling. A fixed cache holds owned rectangles and metadata keyed by source file, frame address, frame index, dimensions and encoded length. It contains no retained game-data buffers or dereferenced borrowed pointers. Its entries plus scratch buffer remain below 1 MiB regardless of exploration size. Source identities refer to immutable DC6 frames in the supported renderer; file/frame/index changes avoid allocation aliasing, and area/session changes invalidate all entries.

`ExplorationRuntime.cpp` uses these bounds only for retained native artwork in active hybrid mode outside the town exemption. It avoids mask queries over the transparent margins and skips completely blank sprites. Original texture generation, source UVs, colors and drawing order remain authoritative. Full canvas metadata is preserved for contact/trace registration. Sewer tracing and sewer water union retain their existing route. Invalid metadata, failed reads or unsupported encoding use the original clipping path. This is an exact padding optimization, not contour simplification or a reduced map refresh rate.

The existing Poisoned Well classification correction, prepared floors, towns, adjoining areas, opacity and style settings remain in place. No new map-specific wall suppression is added here.

## Validation and limits

All three Win32 Release test suites pass, including the optional local-table audit and both 1,974-frame artwork sheets. Tests compare occupied bounds with the independent silhouette decoder for every local frame. Synthetic runtime tests compare visible pixel values and interpolated UVs before/after clipping at both zooms, multiple pans, screen edges and reveal frontiers. They also cover blank artwork, malformed/truncated data, source-read fallback, metadata identity changes, cache reuse and area invalidation. Existing icon/water, town, sewer, adjoining-area and exploration tests continue to pass.

A synthetic panning scene uses the user's actual large-map artwork frames and tables, with a mixture of endgame water, details, icons and ordinary walls. The scene has 1,556,968 explored fine cells, 13,276 floor quads, 1,417 wall runs and 4,615 native cell-hook calls per pass. The final build's median of three paired 180-pass x86 Release runs was **3.70 ms before and 3.12 ms after**, about **15.7% less callback CPU time**. An earlier candidate measured a 20.6% reduction; timing varies between runs. Native quad submissions consistently fell from **1,684,800 to 742,860 (55.9%)** per run. Modern vertex counts and polygon area matched. Some native cells no longer forward when only transparent pixels would have been visible; exact visible-pixel preservation is tested separately.

The benchmark uses mocked renderer callbacks and synthetic geometry. It is not a measurement of in-game FPS, GPU time, or a guarantee that all of the endgame test map's reported drop is recovered. The modern geometry itself still grows with exploration. Other renderers or runtime edits of DC6 frame contents are not covered by the tested immutable-frame profile.

## Build and live check

Use the [standard Win32 Release build and test steps](../README.md#build-from-source). Close Diablo II before installing a candidate DLL and keep the previous DLL/INI as a backup. Current live-test comparisons should finish on their original build before replacing it.

Test a large endgame map in both fullscreen overlay and corner modes, including zoom/pan and a nearly complete exploration. Confirm water/detail pixels and symbols remain intact, then compare late-run FPS and save the log. Recheck Poisoned Well, a sewer, a town gate and adjoining campaign areas. `ARTWORK trimmed`, `blankSkipped`, `boundsHits`, `boundsDecoded` and `boundsFailures` show whether the bounds path is working; they are cumulative counters, not FPS measurements. The beta.3 release includes this optimization; broader performance and compatibility testing continues.
