# PD2 Exploration Automap 1.0

An exploration automap for Project Diablo 2, with shaded floors, wall contours and a colored boundary around explored terrain. Campaign and endgame maps have separate styles, colors and boundary thickness settings.

**[Download v1.0.0 — Windows x86](https://github.com/RoofooEvazan/pd2-exploration-automap/releases/tag/v1.0.0)** · [Settings](#appearance-settings) · [Screenshots](docs/screenshots/) · [Build from source](#build-from-source)

This is an unofficial plugin for the [tested PD2/D2GL binary set](compatibility.json). Testing has been offline; online compatibility and broad version support are not established. Only the plugin DLL is distributed; game and renderer files come from your installation.

## Map styles

Open the automap once in a game, then go to **Options → Automap Options → Maps Styling** or **Campaign Styling**. Each group has its own Boundary Color, Wall Color, Water Color, Boundary Thickness and Stylization. Changes apply immediately in both fullscreen and corner views.

| Style | Appearance |
| --- | --- |
| **Original** | Native game automap and discovery, without an exploration boundary or replacement wall contours. |
| **Native** | Original artwork with exploration clipping and a colored boundary, without custom floor shading or contours. |
| **Hybrid** | Wall contours, shaded floors and an exploration boundary alongside native water patterns, roads, landmarks and icons. Recognized water edges follow Water Color independently of Wall Color. |
| **Styled** | Contours replace terrain artwork. Recognized shrines, event markers, waypoints, entrances, exits, stairs and other navigation artwork remain. |

Native, Hybrid and Styled reveal a smooth circle of **33 world subtiles** around the character. The radius is compiled into the plugin and cannot be changed through settings. Towns retain native drawing without exploration clipping; nearby outdoor terrain follows the selected style. Switching styles preserves exploration and colors.

Original dims neutral white wall pixels by 40% in fullscreen; colored artwork, icons and the corner minimap retain their brightness.

Connected campaign areas on the same native map layer retain their explored terrain across zone transitions. Exploration lasts for the current game session and is not saved to disk. Walls remain visible while new room geometry is prepared. See [map styles](docs/map-styles.md) for transition behavior and legacy INI compatibility.

## Screenshots

Hybrid overlay with native water and landmarks:

![Endgame hybrid overlay](docs/screenshots/endgame-map-hybrid-overlay.png)

Corner minimap:

![Endgame corner minimap](docs/screenshots/endgame-map-hybrid-corner-minimap.png)

The [full gallery](docs/screenshots/) includes campaign areas, sewer channels and boundary colors. These images are from earlier betas; the radius, settings menu and color palette have since changed.

## Install and run

Requirements: an installed copy of Diablo II / Project Diablo 2, the supported D2GL renderer, and the engine files listed in [compatibility.json](compatibility.json).

1. Download the **Windows x86 release ZIP** linked above and extract it. GitHub's automatic source ZIP does not contain a compiled DLL.
2. Check your installation from the extracted folder:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\Check-Compatibility.ps1 -GamePath "D:\Games\Diablo II\ProjectD2"
   ```

   Replace the example path with your game folder. Required engine files must report `MATCH`. The optional menu check controls availability of the in-game settings extension; INI settings remain available on unsupported menu layouts.

3. Close the game. Back up `d2gl.json` and any existing `ExplorationMask.dll` and `ExplorationMask.ini`.
4. Copy `ExplorationMask.dll` and, for a first installation, `ExplorationMask.ini` alongside `Game.exe`. **When upgrading, keep your INI to preserve preferences.**
5. Append this entry to the comma-separated `other.load_dlls_late` string in `d2gl.json`:

   ```text
   ExplorationMask.dll:cdecl:InitExplorationMask
   ```

   For example, an existing `SGD2FreeRes.dll` entry becomes:

   ```json
   "other": {
     "load_dlls_late": "SGD2FreeRes.dll,ExplorationMask.dll:cdecl:InitExplorationMask"
   }
   ```

   Preserve the rest of the JSON and every existing loader entry. Add the plugin only once.

6. Launch with the game folder as the working directory:

   ```text
   Game.exe -3dfx -direct -exploration-test -log
   ```

   For a shortcut, set **Target** to `"D:\Games\Diablo II\ProjectD2\Game.exe" -3dfx -direct -exploration-test -log` and **Start in** to the same game folder. Keep your normal D2GL HD settings. Do not add `-w`; the Glide wrapper rejects it. Keep `-direct` if your setup uses custom loose files.

7. Enter an offline game and open the automap. `ExplorationMask.log` reports initialization, loaded definitions and rendering diagnostics.

The plugin reads tables automatically from the installed game's archives or active direct overrides. No manual extraction or separate table download is needed. See [table loading](docs/game-tables.md) if hooks install but styling is missing.

**Saved preferences:** existing styles continue to load; retired color names resolve to [replacement colors](docs/boundary-settings.md#older-color-settings). Each group inherits legacy preferences until its own keys are saved. Old `MapStyle=native` selects native artwork and discovery, now called Original. Old `RevealRadiusSubtiles` and `RevealMode` entries are ignored and can be removed.

**Uninstalling:** close the game, remove only the plugin's loader entry, and remove its DLL and INI. The DLL is dormant without `-exploration-test`; unloading it from a running game is unsupported.

## Appearance settings

**Boundary Color**, **Wall Color** and **Water Color** open separate lists in each Styling submenu. Select a color by mouse or with Up/Down and Enter. Changes save immediately; Back or Escape cancels.

All three color lists offer Red, Yellow, Green, Cyan (`#00FFFF`), Magenta and White. Boundaries default to Cyan; walls and water edges default to White. Wall Color affects custom contours, and Water Color affects recognized Hybrid water and bank edges. Native artwork and icons keep their colors. [Color values and menu compatibility](docs/boundary-settings.md) are documented separately.

Poisoned Well can supply water references in its custom automap definitions. With those definitions active, Original and Native show dull-green water edges (`#708860`). No custom game definitions are included in the release. Hybrid uses the selected Water Color for shore contours; Styled omits the water artwork. The extra translucent green fill is disabled to reduce drawing cost.

Default `ExplorationMask.ini`:

```ini
[Maps]
Style=styled
BoundaryColor=cyan
WallColor=white
WaterColor=white
BoundaryThickness=1.0

[Campaign]
Style=hybrid
BoundaryColor=cyan
WallColor=white
WaterColor=white
BoundaryThickness=1.0
```

**Boundary Thickness** cycles through 0.5, 1.0, 1.5 and 2.0 in each Styling submenu, saving to that group’s `BoundaryThickness` key. The default is 1.0; older shared INI values are inherited until a group chooses its own. It does not alter the fixed reveal radius or wall width.

The existing **Fade** option controls fullscreen walls and water in Original, Native, Hybrid and Styled. Custom contours and floors follow the same No, Center, Everything and Auto modes. Boundaries retain their base opacity in every Fade mode. The corner minimap retains D2GL's fixed capture opacity. Legacy `StylizationOpacity` and `OverlayOpacity` keys are ignored and can be removed. Manual INI edits require a restart; menu changes do not.

Recognized entrance/exit symbols and cave/stair artwork receive **75% more alpha**, capped at full opacity. Already-solid artwork and the fixed-alpha corner view use a brightness boost instead. See [entrance visibility](docs/entrance-visibility.md) for classification and renderer limits.

## Build from source

Requirements: Windows, CMake 3.20+, Visual Studio 2019+ with **Desktop development with C++**, the MSVC x86 toolchain and a Windows SDK. The default tests need no game assets or game installation.

From the repository directory, using Visual Studio 2022:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

For Visual Studio 2019, use `-G "Visual Studio 16 2019"`. Output must be **32-bit**, even on 64-bit Windows. The DLL is written to `build/Release/ExplorationMask.dll`; builds use C++17, the static MSVC runtime and warnings as errors. A rebuilt DLL may have a different file hash from the release binary.

The three suites check mask geometry, rendering contracts, menu behavior and worker updates. See [testing](docs/testing.md) for optional fixtures and gameplay checks.

## Performance and limitations

Unchanged geometry, clipping results and water perimeters are cached. A background worker builds new terrain while the renderer uses the latest completed result. Drawing still runs each automap pass. [Performance notes](docs/performance.md) record historical benchmarks and remaining costs; they are not FPS guarantees for this release.

- Discovery is distance-based and can reveal through nearby walls. It is not native tile discovery or line of sight. Hardcoding the radius removes its configuration override, but does not prevent modification of an open-source client.
- Contours depend on loaded collision data. Small obstacles may appear, unfamiliar artwork may retain native rendering, and decorative water may lack an outline. Styled can omit roads or bridges that are not classified as navigation artwork.
- New geometry can lag behind movement. Initial builds, large maps, cache eviction and capture limits can affect performance or omit later styled terrain.
- Town exemptions use level rectangles, with one already-loaded outdoor neighbor previewed at a time. The plugin does not load or reveal extra rooms.
- Some independently drawn plugin markers and text bypass the terrain hook. They need separate compatibility checks.
- Long-session stability and full campaign coverage remain under test; see the performance notes for historical issues and measurement limits.

## Repository guide

| Path | Contents |
| --- | --- |
| [src/](src/) | Runtime hooks, exploration, geometry, colors and menus. |
| [tests/](tests/) | Synthetic suites and optional fixture checks. |
| [docs/](docs/) | Architecture, feature guides, benchmarks and screenshots. |
| [scripts/](scripts/) | Read-only installation compatibility checker. |
| [compatibility.json](compatibility.json) | Tested binary fingerprints, loader entry and release hash. |

## Contributing

For a bug report, include the plugin version, engine hashes, renderer settings, map style and reproduction steps. Performance reports should compare early and late exploration in the same area and distinguish `mapMs` from total frame time. Remove personal data before sharing logs; do not upload saves or proprietary game files.

A port to another game build must validate addresses, data layouts, calling conventions and rendering state together. See [architecture](docs/architecture.md) before changing compatibility guards.

## License and acknowledgments

Original source is available under the [MIT License](LICENSE). This license does not cover Diablo II, Project Diablo 2, D2GL, BH or their assets.

[PD2 D2GL](https://github.com/Project-Diablo-2/d2gl), [PD2 BH](https://github.com/Project-Diablo-2/BH) and [D2MOO](https://github.com/ThePhrozenKeep/D2MOO) provided interoperability references. Their implementations are not vendored here. Diablo II and Project Diablo 2 belong to their respective owners and contributors.
