# Quake 2 Rerelease Map Trainer

A training mod for Quake 2 Rerelease that helps players learn maps, improve item timing, and practice movement mechanics.
---

## 🎯 Features

### 📍 **Item Path Training**
Guides you through items on the map in a randomized sequence. Perfect for learning optimal item routes and map layouts.

- Automatically detects all items from the map (no setup required!)
- Shows you which item to collect next
- Customizable: enable/disable weapons, ammo, health, armor, or powerups
- Great for learning new maps quickly

### ⏱️ **Item Timing Training**
Provides precise feedback on your item respawn timing accuracy.

- Tracks armor, weapons, powerups, and megahealth
- Shows if you were early or late (in seconds)
- Special megahealth handling (accounts for decay time)
- **Timing HUD** with weaning levels: Full countdowns → item names only → match clock only → off
- **Match clock** to anchor your timing against, the way a real duel does
- **Silent mode**: suppresses the per-attempt readout so you have to verify yourself; results arrive in the session summary instead
- Runs **alongside** the Path Trainer if you want the split-attention drill
- Perfect for competitive duel practice

### 👻 **Ghost Duel**
A bodiless simulated opponent that contests the map's major items with you.

- **Takes items on a real cycle**: routes between RA / YA / GA / MH / Quad / RL / RG / CG and genuinely consumes them, so the map stops being predictable from your own actions alone
- **Audible**: you hear the item's real pickup sound at its real position, attenuated by distance — exactly the cue real players time armor by
- **Starts your timers**: a ghost pickup begins a timing entry, so returning on schedule is driven by what you *heard*, not by a message
- **Denials both ways**: beat it to an item and it re-routes; let it take one you were tracking and you lose the cycle
- **In control / out of control**: compares your stack against the ghost's and tells you which playbook you're in
- **Skill knob**: Precise (metronome) through Sloppy (±4s arrival jitter)

It has no body and never shoots at you — see *Design and limitations*.

### 🦘 **Jump Trainer**
Practice movement mechanics and difficult jumps.

- **Save/Load Position**: Save spawn points for jump practice
- **Bhop Consistency**: Get real-time feedback on bunny hop timing

### 📊 **Speedometer**
Track your horizontal movement speed from the main menu — useful for strafe jumping and route optimization.

### 🎲 **Spawn Trainer**
Learn spawn point locations and practice spawn awareness.

- **Spawn Bot**: Passive frozen marker that respawns at different spawn points (beacon only — does not fight)
- **Spawn Order**: Choose between vanilla spawn order or random (no repeat)
- **Beacon Beep**: Audio cue to help locate spawn points
- Perfect for learning spawn locations and practicing spawn awareness

---

## 📸 Screenshots

![Screenshot 1](docs/img/1.png)

![Screenshot 2](docs/img/2.png)

![Screenshot 3](docs/img/3.png)

---

## 📥 Installation

1. **Download** the latest release zip from [GitHub Releases](https://github.com/ozy24/q2re-map-trainer/releases) (e.g. `q2re_map_trainer_v1.2.3.zip`)
2. **Extract** the zip — it contains a `maptrain` folder with `game_x64.dll`
3. **Copy** the `maptrain` folder into your Quake 2 `baseq2` directory:
   ```
   <Steam>\steamapps\common\Quake 2\rerelease\baseq2\maptrain\
   ```
   Example:
   ```
   C:\Program Files (x86)\Steam\steamapps\common\Quake 2\rerelease\baseq2\maptrain\game_x64.dll
   ```
4. **Launch** Quake 2 Rerelease
5. Open the console (`~`) and type:
   ```
   game maptrain
   ```
6. Load a deathmatch map (e.g. `map q2dm1`)

**That's it!** The trainer mod is now active.

---

## 🎮 Quick Start

### Activate the mod
After installation, run `game maptrain` in the console before loading a map.

### Opening the Menu
Type `maptrainer` in the console (`~`) to open the trainer menu.

---

## 📖 Usage Guide

### Item Path Training

1. Open the menu (`maptrainer`)
2. Select **"Item Path Trainer"**
3. Enable **"Path Trainer: Disabled"** (toggles to Enabled)
4. Configure item categories (weapons, health, armor, etc.)
5. Close menu and play
6. Pick up the item named on screen, then go to the next one shown

**Tips:**
- Disable item types you don't care about (e.g., turn off ammo for faster routes)
- Use "Combine Health Packs" to treat all health packs as one target
- Item categories follow the SDK item registry (`IF_WEAPON`, `IF_HEALTH`, etc.); mod items with proper flags respect the matching toggle even if their classname is unconventional
- Great for learning optimal item collection routes

### Item Timing Training

1. Open the menu (`maptrainer`)
2. Select **"Item Timing Trainer"**  
3. Enable **"Timing Trainer: Disabled"** (toggles to Enabled)
4. Close menu and play
5. Pick up a major item (armor, weapon, powerup, megahealth)
6. Return to that item's spawn point when you think it's about to respawn
7. You'll see feedback: **"Red Armor: +0.25"** (you were 0.25 seconds late)

**Tips:**
- **Positive numbers** = late (item already spawned)
- **Negative numbers** = early (item hasn't spawned yet)
- **Track**: Toggle **"Track: Major Items"** vs **"All Items"** (major = RL/RG/CG, armors, megahealth, quad)
- **Challenge**: Optional difficulty windows — Easy (±8s), Medium (±5s), Hard (±3s), Pro (±1s); shows SUCCESS/FAILED within the window instead of raw offset alone
- Enable "Free Collect" to pick up armor even when at max (for easier practice)
- Enable "Debug Prints" for detailed timing information

**Megahealth timing**: The trainer automatically handles megahealth's special timing - it waits for your health to decay to 100, THEN starts the 20-second timer.

### Jump Trainer / Bhop Practice

1. Open the menu (`maptrainer`)
2. Select **"Jump Trainer"**
3. Enable features you want:
   - **Save/Load Position**: Practice specific jumps repeatedly
   - **Bhop Consistency**: Get feedback on your bunny hop timing

**Bhop Feedback:**
- **"Perfect"**: Jump within one server tick after landing
- **"Late (Nms)"**: You waited N milliseconds too long on the ground
- **"Early/Held"**: You held jump too early

**Tips:**
- Use savepos/loadpos for practicing difficult jumps
- Bhop trainer only gives feedback during active bhop chains (not your first jump)
- Chain resets after standing still for ~1 second

### Speedometer

Enable from the main menu to see your horizontal movement speed in real-time. Great for optimizing strafe jumping and maintaining speed through turns.

### Spawn Trainer

1. Open the menu (`maptrainer`)
2. Select **"Spawn Trainer"**
3. Enable **"Spawn Trainer: Disabled"** (toggles to Enabled)
4. A bot will spawn and respawn at different spawn points on the map
5. Configure options:
   - **Spawn Order**: Choose "Vanilla" (default game order) or "Random (No Repeat)" (random spawns without repeating)
   - **Beacon Beep**: Enable/disable audio beep to help locate spawn points

**Tips:**
- Use "Random (No Repeat)" to experience all spawn points on a map
- Enable beacon beep to hear where the bot spawns
- Great for learning spawn locations and practicing spawn awareness in duels
- The bot is a passive spawn marker (frozen in place, beacon only) and will automatically respawn when killed

**Player slot:** Spawn Trainer connects as a real client (`"Spawn Trainer"`) and uses one `maxclients` slot. On a full server it may fail to enable; solo or listen-server practice with a spare slot is recommended. The bot still appears on the scoreboard, but killing it does **not** change DM frag counts.

---

## Design and limitations

The Ghost Duel opponent is **deliberately bodiless**: it has no entity, no model, no pathfinding and no combat. The Q2RE SDK ships empty bot AI (`src/bots/bot_think.cpp`) and engine-side navigation that requires per-map `.nav` files, so a genuinely duelling bot is not available to a mod. It is also not what this trains — the hard part of duel item control is the opponent's *consequences* (items gone when you did not take them, a cycle you must re-derive from a sound), not the opponent's aim. Its health/armor are a resource ledger, not a combat model; it never takes damage.

The trainer is built for **one human practitioner plus bots** on a solo or listen server. All trainer configuration and runtime state lives in server-global structures (`level.map_trainer` / `game.map_trainer_config`), and center-print messages broadcast to every connected client. This is intentional — a per-client redesign would only matter if multi-human practice became a goal.

---

## ⚙️ Console Commands

| Command | Description |
|---------|-------------|
| `maptrainer` | Open the trainer menu |
| `savepos` | Save your current position and view angle |
| `loadpos` | Teleport to your saved position |
| `trainer_debug 1` | Enable debug logging to `trainer.log` |
| `trainer_debug 0` | Disable debug logging |
| `trainer_session` | Print the session summary (timing bias, per-item breakdown, control split) |

### Persisted settings (archived cvars)

These are written automatically when you change options in the trainer menu. They survive game restarts.

| Cvar | Default | Description |
|------|---------|-------------|
| `trainer_mode` | `0` | Active trainers, combinable bit flags: `0` off, `1` path, `2` timing, `3` both |
| `trainer_weapons` | `1` | Path trainer: include weapons |
| `trainer_ammo` | `1` | Path trainer: include ammo |
| `trainer_health` | `1` | Path trainer: include health |
| `trainer_armor` | `1` | Path trainer: include armor |
| `trainer_powerups` | `1` | Path trainer: include powerups |
| `trainer_combine_health` | `0` | Path trainer: combine health packs |
| `trainer_speedometer` | `1` | Speedometer enabled |
| `trainer_bhop` | `0` | Bhop consistency trainer |
| `trainer_free_collect` | `1` | Timing: free collect at max armor |
| `trainer_timing_major_only` | `1` | Timing: major items only |
| `trainer_timing_challenge` | `0` | Timing challenge: `0` off, `1`–`4` easy→pro |
| `trainer_spawn_intent` | `0` | Spawn trainer enabled (auto-resumes on map load) |
| `trainer_spawn_random` | `0` | Spawn order: `0` vanilla, `1` random |
| `trainer_spawn_beacon` | `1` | Spawn trainer beacon beep |
| `trainer_ghost` | `0` | Ghost Duel enabled (forces the timing trainer on; disables Path, Jump/bhop, and Spawn trainers) |
| `trainer_ghost_timings` | `1` | Ghost pickups start timing entries (the by-ear drill) |
| `trainer_ghost_skill` | `3` | Arrival jitter: `0` precise, `1` ±1s, `2` ±2s, `3` ±4s |
| `trainer_hud_level` | `3` | Timing HUD: `0` off, `1` clock only, `2` names only, `3` full |
| `trainer_match_length` | `600` | Match clock length in seconds; `0` disables the clock |
| `trainer_silent_feedback` | `0` | Suppress per-attempt timing readout (results at session end) |

---

## 🗺️ Supported Maps

**All Quake 2 maps are supported!** The trainer automatically detects items from the map at runtime - no conversion or setup required.

Works with:
- Official Q2 maps (q2dm1-q2dm8, etc.)
- Community maps
- Custom maps
- Mission pack maps

---

## 🐛 Troubleshooting

### Menu won't open
- Make sure you ran `game maptrain` in the console after launching
- Make sure you're in a multiplayer map (deathmatch mode)
- Open the console (`~`) and type `maptrainer`

### Path training shows "No items found"
- Make sure you're on a deathmatch map (not single-player)
- Check that at least one item category is enabled in the menu

### Timing not working
- Make sure **Timing Trainer** is enabled (check menu)
- Path Training and Timing Training are mutually exclusive (enabling one disables the other)
- You need to be within pickup radius (64 units) of the item spawn for feedback

### Bhop trainer showing incorrect timing
- Make sure you're doing continuous bhops (jumping immediately after landing)
- The chain resets if you stand still for ~1 second
- First jump of a new chain is silent (not counted)

### Debug logging
If you encounter issues, enable debug logging:
```
trainer_debug 1
```
This appends to `trainer.log` in the game's **working directory** (often your Quake 2 install folder, depending on how you launch). Each session writes a timestamped banner; earlier sessions are preserved. Disable with `trainer_debug 0`.

---

## 🔧 Advanced Settings

### Free Collect Mode
Allows you to pick up armor even when at maximum capacity. Useful for timing practice without worrying about your current armor value.

### Combine Health Packs
Treats all health packs (small + medium) as a single target type. Useful for path training when you don't care about specific health pack sizes.

### Debug Prints (Timing)
Shows detailed debug information about timing calculations. Use this if you want to understand exactly how the timing trainer works.

---

## 📊 Technical Details

- **Timing Grace Period**: 5 seconds after pickup (prevents immediate re-timing)
- **Pickup Radius**: 64 units (standard Q2 pickup range)
- **Bhop Chain Timeout**: 1 second of standing still
- **Position Match Tolerance**: 32 units (for item detection)
- **Max Concurrent Timings**: 32 items

### Development

- `build.bat` — builds `dist/game_x64.dll` and `dist/trainer_tests_x64.exe`
- `test.bat` — build + run host unit tests for `trainer_logic` (no engine; same tests run in CI)
- `play.bat` — copies `dist/game_x64.dll` into `baseq2/` for local dev testing (edit paths inside the script; release installs use the `maptrain` mod folder instead)

---

## 🙏 Credits

**Created by**: ozy  
**Version**: v1.2.3  
**Architecture**: "Thin vanilla" design - minimal changes to base game code  

Built with the Quake 2 Rerelease SDK  
Copyright (c) ZeniMax Media Inc.

---

## 📝 Changelog

### v1.2.3 (2026-06-22)
- **Item Path Training** — optional Stimpacks and Armor Shards toggles to exclude minor pickups from path targets

### v1.2.2 (2026-06-22)
- **Item Path Training** — health and armor destinations can be collected at max health/armor so routes are not blocked mid-training
- **Documentation** — README install/usage corrected for the `maptrain` mod folder, `maptrainer` console command, menu labels, and timing challenge behavior

### v1.2.1 (2026-06-20)
- **Spawn Trainer bot placement** — bot stands on spawn points instead of hovering in a jump pose; ground snap runs after final spawn placement and is limited to 32 units so raised spawn platforms are not bypassed to the floor beneath

### v1.2.0 (2026-06-20)
- **Settings persist across game restarts** — archived `trainer_*` cvars restore menu options after exit/relaunch (map-change persistence from v1.1.0 unchanged)
- **Bhop timing uses real seconds** — chain timeout and perfect/late feedback are frame-rate independent
- **Spawn Trainer** — resume retries when a client slot is busy; killing the bot no longer awards DM frags; README documents slot usage
- **Path training** — welcome prompt re-shows after respawn before first pickup; item categories use registry `IF_*` flags (custom items with e.g. `IF_HEALTH` follow the Health toggle)
- **Performance** — path trainer caches item edicts instead of scanning entities each check
- **Robustness** — `trainer_mode_t` centralizes path/timing exclusivity; TagMalloc fail-soft; `trainer.log` appends with session banners
- **Developer** — `trainer_logic` host unit tests (`test.bat`), CI test step, menu row index enums, `g_local.h` prototype cleanup

### v1.1.0 (2026-06-20)
- **Settings now persist across map changes** — trainer configuration is retained when the map changes, and active trainers auto-resume on the new map (path training rebuilds the new map's item list, the spawn bot re-spawns, timings start fresh)
- Build environment overhaul — vcpkg is now a git submodule with a `setup-vcpkg.ps1` bootstrap; `build.bat`/`play.bat` moved to the repo root (build and deploy are separate steps)
- Consolidated version constants into `src/trainer/trainer_version.h` (single source of truth, alongside the repo-root `VERSION` file)

### v0.94 beta
- Various bug and stability fixes

### v0.93 (2025-11-01)
- Refactored to "thin vanilla" architecture
- Fixed bhop chain detection (first jump no longer counted)
- Added centralized debug logging system (`trainer_debug` cvar)
- Fixed item timing duplicate debug messages
- Improved path training feedback
- Tightened position matching tolerance (128→32 units)
- Added version constant for easier updates
- Build output now goes to `dist/` directory

### v0.92
- Added bhop consistency tracking
- Improved megahealth timing logic
- Various bug fixes

### v0.87  
- Initial release
- Path training and timing features
- Jump trainer basics

---

## 🆘 Support

If you encounter bugs or have feature requests, please provide:
1. Description of the issue
2. Steps to reproduce
3. Map name
4. Contents of `trainer.log` (if `trainer_debug 1` was enabled)

---

**Good luck with your training! 🎯**



