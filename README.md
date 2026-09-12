# PD2 Exploration Automap — BETA

**v0.2.0-beta.4 — BETA.** The supplied settings now use a **31-subtile circular reveal radius**, keeping the smooth shaded edge. Choose Red, Neon Green, Magenta, Cyan or Light Blue in the existing Automap Options menu. This release includes a new screenshot gallery and retains hybrid campaign/map styling, earlier rendering optimizations and automatic game-table loading. See [boundary settings](docs/boundary-settings.md) for the radius override and native-average option.

A community experiment that gives **Project Diablo 2** an expanding exploration map: shaded floors, simple gray wall outlines, a soft reveal edge, and colored edges that show where there is still room to explore.

Unexplored terrain stays blank. As you move, the map opens around you. Towns use their normal automap without the exploration mask.

[Download v0.2.0-beta.4 — Windows x86 BETA](https://github.com/RoofooEvazan/pd2-exploration-automap/releases/tag/v0.2.0-beta.4). The ZIP includes the DLL, settings, source, documentation, screenshots and checksums. This is a **prerelease**; compatibility is limited to the profiled PD2/D2GL files, and broader campaign coverage and long-session performance are still being tested.

This is an **experimental Windows x86 plugin for one tested PD2/D2GL binary set**. It has been tested in a local offline game. It is not an official PD2 feature, a general-purpose loader, or a claim of compatibility with online play or other game versions.

## Repository guide

Each folder has its own guide to the files inside. The [screenshot gallery](docs/screenshots/) also includes captions and full-size previews of every map view.

| File or folder | What's inside |
| --- | --- |
| [docs](docs/) | Design notes, performance results, testing instructions, and gameplay screenshots. |
| [scripts](scripts/) | A read-only checker that compares your game files with the tested build. |
| [src](src/) | The plugin's exploration mask, floor shading, wall outlines, renderer hooks, and background updates. |
| [tests](tests/) | Checks for exploration, clipping, drawing behavior, and incremental map updates. |
| [ExplorationMask.ini](ExplorationMask.ini) | Campaign/map style, reveal distance, boundary color and overlay opacity settings. |
| [CMakeLists.txt](CMakeLists.txt) | Build settings for the 32-bit Windows DLL and its three test programs. |
| [compatibility.json](compatibility.json) | Fingerprints of the tested game files and plugin, plus loader and launch settings. |
| [LICENSE](LICENSE) | The MIT license for this project's original source code. |
| [.gitattributes](.gitattributes) | Line-ending rules for source files, documentation, and Windows scripts. |
| [.gitignore](.gitignore) | Files kept out of version control, including local builds, logs, and credentials. |
| [README.md](README.md) | Project overview, screenshots, installation, building, and known limitations. |

## Screenshots

**Endgame map: hybrid overlay.** Modern gray outlines and soft exploration edges sit alongside native blue water patterns, structures and symbols. The translucent overlay keeps the game world visible underneath.

![Endgame map: hybrid overlay](docs/screenshots/endgame-map-hybrid-overlay.png)

**Endgame map: corner minimap.** The upper-right minimap keeps gray contours, shaded floors, native water artwork and neon green exploration edges visible while leaving the center clear.

![Endgame map: corner minimap](docs/screenshots/endgame-map-hybrid-corner-minimap.png)

**Endgame map: rounded reveal.** A freshly revealed patch around the character shows the 31-subtile circular radius and neon green frontier. Only the explored portion of the automap is visible.

![Endgame map: rounded reveal](docs/screenshots/endgame-map-rounded-reveal.png)

**Automap Options: Boundary Color.** Boundary Color appears in the existing Automap Options menu. Select the row to cycle through the five saved colors; this screenshot shows Neon Green.

![Automap Options: Boundary Color](docs/screenshots/automap-options-boundary-color.png)

Compare the same sewer frontier in [Red](docs/screenshots/boundary-color-red.png), [Neon Green](docs/screenshots/boundary-color-neon-green.png), [Magenta](docs/screenshots/boundary-color-magenta.png), [Cyan](docs/screenshots/boundary-color-cyan.png) and [Light Blue](docs/screenshots/boundary-color-light-blue.png). The [full gallery](docs/screenshots/) includes all five previews, sewer overlay and corner views, and earlier campaign/endgame examples.

## Features

- Hybrid campaign styling: gray contours and shaded floors with native roads, entrances, water patterns and icons.
- Straight Act 3 sewer wall profiles, highlighted water channels and retained bridge artwork.
- Five boundary colors in the existing Automap Options menu, saved immediately.
- Hybrid styling for both campaign and maps, independent style settings, and adjustable full-screen overlay opacity.
- Prepared floor coordinates, reused fully explored clips and occupied artwork bounds reduce repeated drawing work.
- Connected campaign areas retain their explored map when crossing zone boundaries.
- Exploration survives automap pauses, temporary loading and travel within the same tracked session.
- Reused artwork clipping and water perimeters reduce repeated CPU work.
- Dark gray floor shading and thin gray wall outlines for endgame maps.
- Muted red reveal edges where explored floor continues into unexplored space; edges against known walls remain gray.
- A quarter-subtile exploration grid, fractional draw coordinates, and seven shade bands for a softer reveal edge.
- A smooth 31-subtile circular reveal in the supplied INI, with optional native-average and legacy 20-subtile modes.
- Native artwork for all five towns, with the exploration style applied to nearby outdoor terrain before crossing a gate.
- Incremental geometry caching and a background worker so a growing map does not require a full rebuild with every movement.
- Primitive clipping of normal automap artwork as a fallback when the styled map is unavailable.

The mask is separate from `Wall1`, `Wall2`, `Wall3`, `Wall4`, and other native automap definitions. No artwork or engine DLL is edited on disk. Styled mode draws its floor and outline replacement during the terrain pass; fallback mode clips the existing artwork before it is submitted to the renderer.

## Compatibility

You need your own installed copy of Diablo II / Project Diablo 2 and the matching D2GL renderer. No game binaries, extracted game assets, saves, or third-party renderer binaries are included here.

Check [compatibility.json](compatibility.json) for SHA-256 hashes of the tested `D2Client.dll`, `D2gfx.dll`, `D2Glide.dll`, and `glide3x.dll`. This beta also requires the listed `D2Win.dll` for menu/session tracking. These hashes identify the actual tested files more precisely than a season or launcher label.

The plugin reads its automap, object and campaign-layer definitions through the already loaded native `Storm.dll`. Your installed PD2 archives supply these automatically. Active `-direct` overrides remain supported. No game tables, additional archive library or extraction utility are bundled or required. Missing or malformed definitions retain the documented native/per-area fallbacks. The checker now also verifies the supported Storm build. See [automatic table loading](docs/game-tables.md).

**Upgrading from an earlier beta:** close the game and replace `ExplorationMask.dll` with the DLL in the beta.4 release ZIP. Add `RevealRadiusSubtiles=31` under `[Automap]` in your existing INI to get the new radius; replacing only the DLL preserves the previous reveal mode. Keep your color and other preferences, loader entry and normal launch flags. Set `MapsStyle=hybrid` if your older INI says `styled` and you want the current map appearance. GitHub's automatic source ZIP does not include the compiled DLL; use the Windows x86 BETA download above.

From PowerShell in this repository, check an installation without changing it:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\Check-Compatibility.ps1 -GamePath "D:\Games\Diablo II\ProjectD2"
```

The runtime also checks hook instructions and renderer entry points. Those checks are limited; they are **not** a complete version or ABI check. Use the listed binary set. An update can disable the plugin or require a source port. See [the hook and architecture notes](docs/architecture.md).

## Install and run

1. Close the game. Keep a copy of your current `d2gl.json` before editing it.
2. Obtain `ExplorationMask.dll` from this repository's release, or [build it below](#build-from-source). Check your engine files against `compatibility.json` first.
3. Copy `ExplorationMask.dll` and `ExplorationMask.ini` into your PD2 game folder, alongside `Game.exe` and `d2gl.json`. If updating an existing INI, merge the settings below to keep your preferences.
4. In `d2gl.json`, append this entry to the comma-separated string `other.load_dlls_late`, keeping every existing entry:

   ```text
   ExplorationMask.dll:cdecl:InitExplorationMask
   ```

   For example, if the existing value is `SGD2FreeRes.dll`, the resulting section includes:

   ```json
   "other": {
     "load_dlls_late": "SGD2FreeRes.dll,ExplorationMask.dll:cdecl:InitExplorationMask"
   }
   ```

   This is a fragment: preserve the rest of the JSON and its existing settings.

5. Launch `Game.exe` with the game folder as the working directory:

   ```text
   Game.exe -3dfx -direct -exploration-test -log
   ```

   For a Windows shortcut, the **Target** should be `"D:\Games\Diablo II\ProjectD2\Game.exe" -3dfx -direct -exploration-test -log` and **Start in** should be `D:\Games\Diablo II\ProjectD2` (replace that path with yours). Use your normal D2GL HD settings. **Do not add `-w`: the D2GL Glide wrapper rejects it.**

6. Enter an offline game, open the automap, and explore a non-town area. `ExplorationMask.log` in the game folder reports whether initialization and hooks succeeded.

### Appearance settings

Settings are read at game startup:

```ini
[Automap]
CampaignStyle=hybrid
MapsStyle=hybrid
OverlayOpacity=80
RevealMode=native-average
RevealRadiusSubtiles=31
BoundaryColor=red
```

`hybrid` combines modern contours with native campaign details. `native` keeps native artwork with exploration shading, `styled` uses simplified terrain, and `original` disables the exploration effect for that group. Campaign and endgame settings are independent. `OverlayOpacity` accepts 10-100; 80 uses 20% less alpha for custom full-screen drawing, and 100 restores the previous opacity. Native symbols keep their own rendering. The tested D2GL corner minimap uses fixed capture opacity and is unaffected. Restart after edits. See the [campaign guide](docs/campaign-prototype.md) for details.

Open the automap once in an offline game, then use **Options → Automap Options → Boundary Color** to cycle Red, Neon Green, Magenta, Cyan and Light Blue. Each selection changes the edge immediately and saves the INI. `BoundaryColor` also accepts `red`, `neon-green`, `magenta`, `cyan` or `light-blue` directly; restart after manual INI edits. The menu extension supports the profiled `ProjectDiablo.dll`; on other menu layouts, color selection remains available through the INI. See [boundary settings and compatibility](docs/boundary-settings.md).

`RevealRadiusSubtiles=31` sets a fixed circular radius of 31 world subtiles, independent of display resolution. It accepts whole values from 1 to 256; movement and the mask still use quarter-subtile precision. This overrides `RevealMode`. Set the radius to `0` to use `RevealMode=native-average` (a circular approximation of native logical-view reach) or `RevealMode=circle` (the legacy 20-subtile circle). A missing or invalid radius also follows `RevealMode`. Towns remain fully explored. Restart after editing the radius. This is distance-based exploration, not exact native tile discovery or line of sight.

The DLL remains dormant without `-exploration-test`. To remove it completely, close the game, remove only its entry from `other.load_dlls_late`, and delete `ExplorationMask.dll` and its INI if no longer needed. Restart the game after any change; live unloading is unsupported.

## Build from source

Requirements: Windows, CMake 3.20 or newer, and Visual Studio 2019 or newer with **Desktop development with C++**, the MSVC x86 toolchain, and a Windows SDK. No game SDK or game assets are required to compile or run the default tests.

For Visual Studio 2022, run these commands from the repository directory:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

For Visual Studio 2019, use `-G "Visual Studio 16 2019"`. CMake must be on your PATH; the Visual Studio CMake component also supplies it. The game requires **32-bit** output even on 64-bit Windows.

The resulting module is `build/Release/ExplorationMask.dll`. CMake uses C++17, the static MSVC runtime, and warnings as errors. A fresh build is not expected to have the same hash as the supplied tested DLL.

The three test executables cover the mask, native primitive clipping, fractional rendering, town footprints and gate transitions, shading and red-frontier coverage, cached-versus-full geometry equivalence, and worker snapshot handling. They use synthetic data and mocked renderer calls; they do not establish compatibility with a running game. Optional local integration checks are described in [testing.md](docs/testing.md).

## Performance

The renderer rebuilds changed regions on a single background worker and draws the latest completed geometry. In one development test with more than 1.4 million explored fine cells, sampled worker builds averaged about **11.4 ms** and automap CPU work averaged about **1.45 ms**, with an observed 240 FPS game display. Earlier full rebuilds in that test had grown to roughly 300 ms.

Those historical endgame measurements do not establish beta campaign FPS. The beta's clipping/water optimization reduced synthetic CPU rendering time by about 21% in the sewer scene, 40% in a town-preview scene and 19% outdoors, with matching geometry counts and checksums. These are benchmark results, not guaranteed FPS gains. Beta.3 additionally reuses prepared floor coordinates, retains fully explored clipping results, removes duplicate Poisoned Well contours, and avoids clipping transparent artwork margins. See [large-map improvements](docs/map-growth-optimization.md), [Endgame artwork optimization](docs/native-artwork-padding.md), [hybrid performance](docs/hybrid-performance.md) and [earlier measurements](docs/performance.md) for scope and remaining costs.

## Limitations

- Exploration is distance-based, not the game's native revealed state or a line-of-sight simulation. It can reveal through nearby walls.
- Native town artwork is retained within the town's level rectangle. Outdoor previews use already-loaded floor data for one neighboring area at a time; the plugin does not force room loading or reveal every town room. Irregular town footprints may need a more detailed boundary.
- Exploration is not saved to disk. Menu returns, changed player identity, or a changed seed in a previously visited act reset the session. Automap pauses and temporary loading do not reset it by themselves.
- The soft edge uses discrete shade bands, not a continuous blur. Renderer behavior still affects the final color and smoothness.
- Collision-derived outlines can include small obstacles. Sewer water receives a specific enhancement, but other decorative water may lack a modern contour. Hybrid styling protects recognized native details; styled mode replaces some cell-based feature artwork. Separately drawn plugin markers and text are not all covered by the mask.
- New geometry can briefly lag behind movement. Initial area builds, very large maps, and cache eviction can still cost more time.
- This is a BETA. Long-session stability and broad campaign coverage are not established. An older test hit a D2Glide texture-cache assertion; its cause was not confirmed, and this release does not claim to resolve that engine failure.

## Contributing

Bug reports, performance measurements, and patches are welcome. Include the profiled engine hashes, renderer configuration, whether the issue occurs offline, and clear reproduction steps. Review logs before sharing them; do not upload saves, proprietary game files, or personal information.

For a new game build, port and validate the addresses, structures, calling conventions, and render-state assumptions together. Do not simply remove the compatibility guards. The code layout and current interception points are in [architecture.md](docs/architecture.md).

## License and acknowledgments

The original source in this repository is available under the [MIT License](LICENSE). This license does not cover Diablo II, Project Diablo 2, D2GL, BH, or their assets.

Thanks to the [PD2 D2GL](https://github.com/Project-Diablo-2/d2gl), [PD2 BH](https://github.com/Project-Diablo-2/BH), and [D2MOO](https://github.com/ThePhrozenKeep/D2MOO) projects for publicly documented interoperability references. Their code and licenses remain separate; this repository does not vendor their implementations. Diablo II and Project Diablo 2 belong to their respective owners and contributors.
