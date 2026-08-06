# Markdown++ MSI installer

A WiX v5 project that builds the x64 MSI for Markdown++.

The installer:

- Auto-detects the Notepad++ install directory from `HKLM\SOFTWARE\Notepad++`
  (64-bit registry view) and installs into `<Notepad++>\plugins\MarkdownPlusPlus`.
- Refuses to install with a clear message when 64-bit Notepad++ is missing,
  distinguishing "only 32-bit Notepad++ found" from "not found at all".
- Prompts to close Notepad++ if it is running (it locks the plugin DLL).
- Upgrades and uninstalls cleanly through Windows Installer.

## Files

| File | Purpose |
| --- | --- |
| `Package.wxs` | Installer authoring (registry search, guards, harvest, UI). |
| `license.rtf` | MIT license shown on the welcome screen. |
| `build-installer.ps1` | Builds the MSI from a packaged plugin folder. |

## Build

```powershell
dotnet tool install --global wix --version 5.0.2   # one-time
powershell -ExecutionPolicy Bypass -File .\build-installer.ps1
```

See [`../BUILDING_NATIVE.md`](../BUILDING_NATIVE.md) for options and CI details.
