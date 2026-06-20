# Quake 2 Rerelease Map Trainer — Build Setup

This guide covers building the map trainer game DLL from source on Windows.

## Prerequisites

1. **Visual Studio 2022** (Community or Build Tools) with:
   - C++ build tools workload
   - MSVC v143
   - Windows 10/11 SDK
2. **Git** (for cloning and vcpkg submodule)

## First-time setup

From the repo root:

```powershell
git submodule update --init --recursive
.\scripts\setup-vcpkg.ps1
```

This initializes the [vcpkg](https://github.com/microsoft/vcpkg) submodule, bootstraps the tool, and installs manifest dependencies (`fmt`, `jsoncpp`) into `src/vcpkg_installed/`.

Dependencies are declared in [`src/vcpkg.json`](vcpkg.json) and linked automatically via MSBuild manifest mode (`Directory.Build.props` at repo root).

## Building

From the repo root:

```cmd
.\build.bat
```

Release build:

```cmd
set MUFFMODE_BUILD_CONFIG=Release
.\build.bat
```

Output: [`../dist/game_x64.dll`](../dist/game_x64.dll)

`build.bat` will:
- Verify the vcpkg submodule is present and bootstrapped
- Install manifest dependencies if `src/vcpkg_installed/` is missing
- Locate Visual Studio via `vswhere` and run MSBuild on `src/game.sln`

## Deploy and run locally

After a successful build:

```cmd
.\play.bat
```

This copies `dist/game_x64.dll` to your Quake 2 install and launches the game. Edit `play.bat` if your Steam install path differs from the default.

## Troubleshooting

### vcpkg submodule missing

```powershell
git submodule update --init --recursive
.\scripts\setup-vcpkg.ps1
```

### Missing fmt or jsoncpp headers/libs

Re-run setup:

```powershell
.\scripts\setup-vcpkg.ps1
```

Or manually from `src/`:

```powershell
$env:VCPKG_ROOT = "..\vcpkg"
..\vcpkg\vcpkg.exe install --triplet x64-windows-static
```

### Visual Studio not found

Install VS 2022 with the C++ workload, or ensure `vswhere` can find an installation with MSBuild.

### play.bat cannot find the DLL

Build first with `.\build.bat`. The DLL is written to `dist/game_x64.dll`, not the repo root.

## Resources

- [Project README](../README.md)
- [id Software Quake 2 Rerelease Repository](https://github.com/id-Software/quake2-rerelease-dll)
