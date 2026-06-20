# Q2RE Map Trainer — Improvement & Bugfix Recommendations

A prioritized list of potential bugs, robustness gaps, and enhancement ideas for the mod.
Each item is tagged **[Confirmed]** (observed directly in the code) or **[Potential]** (plausible
from reading, but not verified at runtime). Effort is a rough estimate.

> **Status:** The original weakness #1 — *settings reset on every map change* — was fixed in
> **v1.1.0** (config now persists in `game.map_trainer_config` and auto-resumes per map). It is
> listed below under "Already addressed" for context.

> **Design scope:** The trainer targets a **single human practitioner plus bots** (solo / listen
> server), not multiple concurrent human players. Server-global trainer state is therefore an
> intentional design choice, not a defect — recommendations below are scoped to that use case.

---

## High priority — correctness & robustness

### 1. Trainer state is global, not per-client **[Confirmed]** — *by design — no action needed*
All trainer config and runtime live in `level.map_trainer` / `game.map_trainer_config`, which are
**server-global**, and center-print messages (`MapTrainer_PickNewTarget`,
`MapTrainer_ShowWelcomeMessage`) broadcast to all clients. **This is intentional:** the mod is built
for a single human practitioner plus bots, so shared global state is the correct, simpler model.
- **No refactor recommended.** A per-client rewrite (state in `gclient_t`) would only matter if
  multi-human practice ever becomes a goal — out of scope today.
- **Worth doing cheaply:** state the single-practitioner assumption in the README so the constraint is
  explicit to users and future contributors.

### 2. Config doesn't survive a game restart or real savegame **[Partially addressed]**
`game.map_trainer_config` persists across **map changes** (the `game` struct stays in memory in
deathmatch) but:
- **Restart — done:** archived `trainer_*` cvars (`trainer_cvars.cpp`, `CVAR_ARCHIVE`) persist menu
  settings across process exit; see README → "Persisted settings". `cfg.valid` gates the in-session
  `game` struct overlay on map changes while cvars remain the source of truth on a fresh load.
- **Savegame — open (low value for DM):** `map_trainer_config` is still **not** registered in
  `game_locals_t_savestruct` (`g_save.cpp`), so Write/ReadGame (campaign/unit transition, autosave)
  will not serialize it. Add the field to the savestruct only if campaign savegame fidelity is wanted.

### 3. Bhop timing is frame-rate dependent **[Addressed]**
`MapTrainer_UpdateBhopTracking` (`trainer_jump.cpp`) now uses `bhop_landing_time` (`gtime_t`) and
real-second thresholds (`BHOP_CHAIN_TIMEOUT_SEC`, one server tick via `gi.frame_time_ms`) instead of
frame counts. Feedback shows late timing in milliseconds.

### 4. Spawn-bot auto-resume gives up after one failed attempt **[Confirmed]** — *low effort*
`MapTrainer_RunFrame` clears `spawn_trainer_resume_pending` whether or not
`MapTrainer_EnableSpawnTrainer(nullptr)` actually succeeded (e.g. no free client slot at that moment).
If the slot frees up later, the bot won't come back even though intent is still set.
- **Fix:** only clear `resume_pending` on success; on failure leave it pending (optionally with a
  short retry backoff) so it re-arms once a slot is available.

### 5. Item-availability scan is O(entities) per call **[Addressed]**
`map_trainer_item_t` caches the map `edict_t*` at build time; `MapTrainer_IsItemAvailable(item_index)`
checks `SVF_RESPAWNING`/`solid` directly with no `G_FindByString` scan.

### 8. Item classification is string-prefix matching **[Addressed]**
`MapTrainer_IsItemCategoryEnabledForItem` uses `ent->item->flags` (`IF_WEAPON`, `IF_AMMO`, etc.) with
classname fallback for virtual/combined classes and mod items. Armor names unified across both
`item_armor_*` and `item_*_armor` conventions.

---

## Medium priority — maintainability & UX

### 6. Path/Timing mutual exclusion is two bools kept in sync by hand **[Confirmed]** — *low effort*
`training_enabled` and `timing_enabled` must never both be true; today each toggle manually disables
the other and clears its state (`trainer_menu.cpp`). This is duplicated in several places and easy to
desync.
- **Fix:** replace the two booleans with a single `trainer_mode` enum (`OFF / PATH / TIMING`) and
  centralize the teardown of the previous mode in one switch.

### 7. Menu uses fragile hard-coded entry indices **[Confirmed]** — *low/medium effort*
Update functions index entries by literal position (`entries[7]` for the speedometer line,
`entries[2..8]` for categories, etc.). Re-ordering a menu silently breaks the wrong row.
- **Fix:** name the indices via an `enum`/`constexpr`, or look entries up by a stable id rather than
  position.

### 9. Magic numbers scattered through the trainers **[Confirmed]** — *low effort*
Pickup radius (64), position-match tolerance (32), timing grace period (5s), bhop timeout (30 frames),
max concurrent timings (32) appear as literals across files (some are documented in the README's
"Technical Details").
- **Fix:** hoist into named `constexpr` (or cvars where a server admin might want to tune them) in one
  place, e.g. a `trainer_config.h`.

### 10. Hardcoded defaults with no cvar overrides **[Confirmed]** — *low effort*
`MapTrainer_Init` sets every default in code. There's no way for a server/admin to change the starting
state (e.g. start with speedometer off, or major-items-only off) without recompiling.
- **Fix:** expose the initial defaults as archived cvars read in `MapTrainer_Init` (before
  `LoadConfig` overlays any saved values).

### 11. Spawn bot consumes a real player slot **[Confirmed/Potential]** — *medium effort*
The spawn trainer creates a genuine client via `ClientChooseSlot`/`ClientConnect`, occupying one of
`maxclients`. On a full or low-slot server this can fail or displace a player; the bot also shows in
scoreboard/player counts.
- **Fix:** document the slot requirement; consider reserving/ensuring a slot, and verify the bot is
  excluded from DM scoring/endlevel logic.

---

## Lower priority — polish

### 12. `trainer.log` is opened with a relative path, truncated each run **[Confirmed]** — *trivial*
`trainer_debug.cpp` opens `"trainer.log"` in the process working directory in truncate mode. Multiple
runs overwrite; location depends on how the game is launched.
- **Fix:** make the path/append behavior explicit, or note it in the README troubleshooting section
  (already partly documented).

### 13. No automated verification **[Confirmed]** — *medium effort*
Everything is validated manually in-game. The pure-logic pieces (target selection, category filtering,
timing math, version parsing) could be unit-tested.
- **Fix:** factor the testable logic behind thin interfaces and add a small host-compiled test target
  (no engine), runnable in CI alongside the build.

### 14. TagMalloc return values are assumed non-null **[Potential]** — *low effort*
Item/unique-item allocation in `trainer_core.cpp` uses `gi.TagMalloc(...)` and writes into it without
checking for null. The SDK typically aborts on OOM, so this is low-risk, but worth a guard if you want
to fail soft.

### 15. Welcome-message / first-pickup flow is implicit **[Potential]** — *low effort*
Path training relies on `first_pickup` and a welcome center-print to bootstrap. Edge cases (player
joins mid-map, dies before first pickup, map has exactly one item type) deserve a quick pass to make
sure the prompt always appears and a target is always reachable.

---

## Already addressed (for reference)

- **Settings reset on every map change** → fixed in v1.1.0 (config/runtime split,
  `MapTrainer_SaveConfig`/`LoadConfig`, per-map auto-resume of active trainers). See
  `CLAUDE.md` → "State model and lifecycle".
- **Spawn-bot creation during map spawn was unsafe** → deferred to `MapTrainer_RunFrame` on a normal
  server frame.
- **`MapTrainer_EnableSpawnTrainer(nullptr)` crash risk** → null-`requester` prints are now guarded.
- **Settings lost on process exit** → archived `trainer_*` cvars (`trainer_cvars.cpp`); restart case
  covered. Campaign savegame serialization via `game_locals_t_savestruct` still deferred (low value
  for DM).
- **#4 spawn-bot resume gives up after one failed attempt** → retry with backoff (`trainer_spawn.cpp`).
- **#6 path/timing mutual exclusion** → `trainer_mode_t` + `MapTrainer_SetMode()` (`trainer_core.cpp`).
- **#9 magic numbers** → `trainer_config.h`.
- **#1 single-practitioner assumption** → README "Design and limitations".
- **#3 frame-rate-independent bhop timing** → `bhop_landing_time` + `gtime_t` thresholds (`trainer_jump.cpp`).
- **#5 item edict cache** → `map_trainer_item_t::ent`, direct availability check (`trainer_path.cpp`).
- **#8 item flags classification** → `MapTrainer_IsItemCategoryEnabledForItem` (`trainer_core.cpp`).

---

## Suggested order of attack

1. ~~#4 spawn-bot resume retry~~ — done.
2. ~~#6 mode enum + #9 magic-number hoist~~ — done.
3. ~~#2 restart persistence (cvars)~~ — done; savegame half optional.
4. ~~**#3** frame-rate-independent bhop timing~~ — done.
5. ~~**#5** item-availability caching~~ and ~~**#8** item classification~~ — done.
6. Longer term: **#13** automated tests; **#7** menu index enums; polish items **#11–#15**.

> Note: #1 (per-client state) is intentionally **not** on this list — global state is by design for
> the single-practitioner use case.
