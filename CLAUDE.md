# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A training mod for **Quake 2 Rerelease (Q2RE)** built as the game DLL (`game_x64.dll`) from the Q2RE C++ SDK. It adds item-path training, item-timing practice, a jump/bhop trainer, a speedometer, and a spawn trainer (a bot that respawns at spawn points). The output DLL drops into a Q2RE `baseq2/` folder and runs in deathmatch.

## Build

Requires **Windows**, **Visual Studio 2022** (Community or Build Tools, MSVC v143), and **Git**. Dependencies (`fmt`, `jsoncpp`) come from **vcpkg manifest mode**: [`src/vcpkg.json`](src/vcpkg.json) + MSBuild imports in [`Directory.Build.props`](Directory.Build.props) / [`Directory.Build.targets`](Directory.Build.targets). The vcpkg **tool** is a git submodule at [`vcpkg/`](vcpkg/); manifest installs land in `src/vcpkg_installed/` (gitignored).

### First-time setup (fresh clone)

```powershell
git submodule update --init --recursive
.\scripts\setup-vcpkg.ps1
```

`setup-vcpkg.ps1` bootstraps `vcpkg/vcpkg.exe` and runs `vcpkg install --triplet x64-windows-static` from `src/` (matches `VcpkgUseStatic=true` in [`src/game.vcxproj`](src/game.vcxproj)). More detail: [`src/BUILD_SETUP.md`](src/BUILD_SETUP.md).

### Build and deploy

From the **repo root**:

```cmd
REM Default: Debug|x64
.\build.bat

REM Release (what CI builds)
set MUFFMODE_BUILD_CONFIG=Release
.\build.bat

REM Optional args: platform, toolset (defaults: x64, v143)
.\build.bat x64 v143
```

- **Output:** `dist/game_x64.dll` (and `.pdb` / `.lib` / `.exp` alongside it).
- **`build.bat`** finds VS via `vswhere`, checks the vcpkg submodule is initialized/bootstrapped, auto-runs manifest install if `src/vcpkg_installed/` is missing, then builds `src/game.sln`.
- **`play.bat`** copies `dist/game_x64.dll` into your Q2RE `baseq2/` folder and launches the game — edit the hardcoded Steam paths in `play.bat` if your install differs (build and deploy are separate steps).

Direct MSBuild (after vcpkg setup, from a VS x64 dev prompt):

```cmd
msbuild src\game.sln /m /p:Configuration=Release /p:Platform=x64
```

CI: [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml) — submodule checkout, vcpkg bootstrap, Release build, uploads `dist/game_x64.dll`.

**Mod version:** canonical source is [`src/trainer/trainer_version.h`](src/trainer/trainer_version.h) (in-game menu footer) and repo-root [`VERSION`](VERSION). Bump both plus `README.md` when releasing.

There is **no test suite**. Verification is manual, in-game (see below).

## Manual verification (no automated tests)

1. Build (`.\build.bat`), deploy (`.\play.bat` or copy `dist/game_x64.dll` into `<Q2RE>/baseq2/`), launch deathmatch (e.g. `+set deathmatch 1 +map q2dm1`).
2. Press **TAB** (or console `maptrainer`) to open the menu; toggle features and observe behavior.
3. For logs: console `trainer_debug 1` writes `trainer.log` (truncated each run) in the Q2 working dir. All trainer logging goes through `TrainerLog(category, fmt, ...)` in `trainer/trainer_debug.cpp` and is a no-op unless the `trainer_debug` cvar is set.

## Architecture: "thin vanilla"

This is the single most important convention in the repo. All trainer logic lives in **`src/trainer/`**; the vanilla Q2RE SDK files (`g_*.cpp`, `p_*.cpp`, `m_*.cpp`) are kept as close to upstream as possible so SDK updates merge cleanly. See `docs/THIN_VANILLA_PRINCIPLES.md`.

**Rules when editing:**
- Put new behavior in `src/trainer/`, not in vanilla files.
- The only thing allowed in vanilla files is a **minimal hook**: a `MapTrainer_*` call or a small conditional, each tagged with a `// [Map Trainer]` comment. Do not move logic into vanilla files, and do not change vanilla function signatures.
- The public surface between vanilla and trainer code is **`src/trainer/trainer.h`** — every function vanilla calls is declared there.

### Trainer module layout (`src/trainer/`)
- `trainer.h` — all data structures and the full `MapTrainer_*` API. `map_trainer_t` (per-map state) and `map_trainer_config_t` (persistent config) live here.
- `trainer_core.cpp` — `MapTrainer_Init`, item-list building from live map entities, category filtering, config save/load.
- `trainer_path.cpp` — item path trainer (pick next target, pickup handling, welcome message).
- `trainer_timing.cpp` — item respawn timing trainer (up to 32 concurrent timings, megahealth decay handling).
- `trainer_jump.cpp` — speedometer, `savepos`/`loadpos`, bhop consistency tracking.
- `trainer_spawn.cpp` — spawn trainer bot lifecycle (creates/destroys a bot client, beacon beep).
- `trainer_menu.cpp` — the entire PMenu-based menu/submenu UI and all toggle handlers.
- `trainer_debug.cpp` — centralized `trainer.log` logging gated by the `trainer_debug` cvar.

### State model and lifecycle (critical)
Trainer state is split across two SDK structs with very different lifetimes — get this wrong and you leak, double-free, or lose settings:

- **`level.map_trainer`** (`map_trainer_t`, inside `level_locals_t`): per-map runtime state. The entire `level` struct is `memset` to zero on every map load (`g_spawn.cpp`), and `gi.FreeTags(TAG_LEVEL)` frees all `TAG_LEVEL` allocations *before* entities respawn. Item arrays (`items`, `unique_items`) are `gi.TagMalloc(..., TAG_LEVEL)`, so they are auto-freed each map. `MapTrainer_Init()` is called as the **last** line of `SpawnEntities()` (`g_spawn.cpp:1277`), after items exist.
- **`game.map_trainer_config`** (`map_trainer_config_t`, inside `game_locals_t`): persistent config that survives map changes (`game` is only reset at `InitGame`). Declared via the `#include "trainer/trainer.h"` placed just above `game_locals_t` in `g_local.h`.

Settings persistence works by **mirroring**: `MapTrainer_SaveConfig()` copies the config fields out of `level.map_trainer` into `game.map_trainer_config` (called from every menu toggle, because `PMenu_Update` is throttled and can't be the save hook); `MapTrainer_LoadConfig()` copies them back during `MapTrainer_Init()`, overlaying the hardcoded defaults. Only *config* is mirrored — never runtime state (item arrays, current target, live timings, bot edicts), which is rebuilt per map. When persisting new state, decide explicitly whether it is config (add to `map_trainer_config_t` + both mirror helpers) or per-map runtime (lives only in `map_trainer_t`).

### Key integration hooks in vanilla files
Find them by searching `// [Map Trainer]` or `MapTrainer_`:
- `g_spawn.cpp` — `MapTrainer_Init()` at end of `SpawnEntities`.
- `g_main.cpp` — `MapTrainer_RunFrame()` once per server frame (deferred spawn-bot auto-resume; bot creation is unsafe during `SpawnEntities`).
- `g_items.cpp` — pickup hooks: allow picking up the current target item even at max ammo/health/armor, and route pickups to `MapTrainer_OnItemPickup` / `MapTrainer_OnTimingItemPickup`.
- `p_client.cpp` — per-frame `MapTrainer_UpdateSpeedometer` / timing checks / bhop tracking; spawn-trainer bot client begin/disconnect/respawn; `MapTrainer_ShowWelcomeMessage`.
- `g_cmds.cpp` — registers the `maptrainer`, `savepos`, `loadpos` console commands.

### Menu system
The UI reuses the SDK's PMenu system (`ctf/p_ctf_menu.cpp`): menus are static `pmenu_t[]` arrays with select-callbacks and an update function. Note `PMenu_Update` is rate-limited (~1s), so do not rely on it for side effects that must happen immediately on a toggle — do those directly in the toggle handler.

### Mutual exclusions / gotchas
- Path Trainer and Timing Trainer are mutually exclusive; enabling one disables the other (and frees/clears the other's state).
- `MapTrainer_BuildItemList` frees and rebuilds `items`; it must only run when `items` is `nullptr` or validly allocated — `MapTrainer_Init` nulls it first to avoid a double free.
- The spawn trainer creates a real bot client via `ClientChooseSlot`/`ClientConnect`/`ClientBegin`; this must run on a normal server frame, not during map spawn.
