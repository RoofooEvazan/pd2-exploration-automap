# Game compatibility checker

[Check-Compatibility.ps1](Check-Compatibility.ps1) compares five DLLs in your game folder with the tested SHA-256 fingerprints in [compatibility.json](../compatibility.json), including the BETA's D2Win dependency. It only reads the files; it does not install or patch the plugin. The optional local game tables used by campaign styling are not checked by this script.

Run from the repository folder, replacing the example path with your PD2 installation:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\Check-Compatibility.ps1 -GamePath "D:\Games\Diablo II\ProjectD2"
```

| Result | Meaning |
| --- | --- |
| `MATCH` | That file matches the tested build. |
| `DIFFERENT` | The file exists, but its contents differ from the tested build. |
| `MISSING` | The expected file was not found in the supplied folder. |

All five files must match. Exit code `0` means they match; `2` means at least one file differs or is missing. An invalid path or read error stops the script with an error. The runtime also checks its hooks when the game starts.

Continue with the [installation instructions](../README.md#install-and-run) after checking compatibility.
