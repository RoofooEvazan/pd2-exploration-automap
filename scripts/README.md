# Game compatibility checker

[Check-Compatibility.ps1](Check-Compatibility.ps1) compares the engine DLLs in your game folder with the SHA-256 fingerprints in [compatibility.json](../compatibility.json). Beta.2 adds native `Storm.dll` to the five existing engine/renderer files so game tables can load automatically. It only reads files; it does not install or patch the plugin. Table availability and schema are checked by the runtime after entering a game.

Run from the repository folder, replacing the example path with your PD2 installation:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\Check-Compatibility.ps1 -GamePath "D:\Games\Diablo II\ProjectD2"
```

| Result | Meaning |
| --- | --- |
| `MATCH` | That file matches the tested build. |
| `DIFFERENT` | The file exists, but its contents differ from the tested build. |
| `MISSING` | The expected file was not found in the supplied folder. |

All required engine files must match. Exit code `0` means they match; `2` means at least one required file differs or is missing. Beta.3 also reports the optional PD2 menu fingerprint; a mismatch leaves INI color selection available and does not change the checker exit code. An invalid path or read error stops the script with an error. The runtime also checks its hooks and table-reader bindings.

Continue with the [installation instructions](../README.md#install-and-run) after checking compatibility.
