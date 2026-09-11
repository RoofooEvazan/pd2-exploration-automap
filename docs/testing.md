# Building and testing

The [README](../README.md#build-from-source) gives the CMake commands. All three default tests run without a game installation, external fixture, or network access. Configure a Release **Win32** build with MSVC and run CTest.

| Test | Main checks |
| --- | --- |
| `mask_tests` | Independent explored cells, movement, frontier extraction, clipping and session behavior |
| `runtime_tests` | Mocked native/Glide callbacks, quad coverage and UV interpolation, contact decoding, fractional lines, styled color restoration, exception cleanup and five town bypasses |
| `styled_tests` | Reference erosion, exact shade coverage, viewport clipping, red open edges versus gray wall edges, collision connectivity, chunk/full equivalence, resets, teleports, worker coalescing and session isolation |

Warnings are errors. Tests check actual geometric coverage and renderer-call contracts rather than loading a DLL into the game. Their success does not validate undocumented engine addresses or long-session behavior.

## Optional local artwork check

If you have legally obtained and already extracted `MaxiMap.dc6` and `MaxiMapS.dc6`, point `PD2_AUTOMAP_ARTWORK_DIR` at their directory. `runtime_tests` then decodes all frames in both files in addition to the synthetic tests:

```powershell
$env:PD2_AUTOMAP_ARTWORK_DIR = 'D:\LocalFixtures\AUTOMAP'
ctest --test-dir build -C Release -R runtime_tests --output-on-failure
Remove-Item Env:\PD2_AUTOMAP_ARTWORK_DIR
```

Unset means skip that optional check. A supplied directory with missing or malformed files fails. Do not add the sheets to this repository or a release.

## Optional development collision fixture

`PD2_FLOOR_PROBE` can point to the original local Dark Temple development capture. This legacy integration check uses fixed seed coordinates from that capture, so an arbitrary room dump is not a substitute. The fixture is not distributed or required; the default synthetic suite covers the same floor/geometry algorithms without game data. The test does not write preview files.

## Runtime diagnostics

The game folder's `ExplorationMask.log` records initialization status, hook activity, mask size, CPU timing, and geometry counts. Missing modules or an unexpected hook chain produces a `Not installed` message. An absent `-exploration-test` flag produces `Dormant`.

Useful timing fields:

- `mapMs`: CPU time between the paired automap callbacks, not GPU frame time or a whole-game frame measurement.
- `workerBuildMs`: time spent on the most recently completed background build.
- `revealLatencyMs`: time from that request being queued to its completion.
- `rebuiltChunks` / `totalChunks`: affected regions rebuilt versus retained regions.

Periodic log lines can repeat the most recent worker result when the player is still. Do not interpret every line as a separate rebuild. Review diagnostics before posting them and never include game saves, account data, or proprietary assets.
