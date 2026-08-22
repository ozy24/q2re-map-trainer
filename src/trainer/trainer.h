// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// ==================== MAP TRAINER SYSTEM ====================
// Q2RE Map Trainer - Training mod for learning maps and practicing jumps
// Thin vanilla architecture: all trainer-specific logic isolated here

#pragma once

// Forward declarations from g_local.h
struct edict_t;
struct pmenuhnd_t;
struct pmove_t;
struct gitem_t;

// ==================== DATA STRUCTURES ====================

// Timing Challenge difficulty levels
enum class timing_challenge_mode_t : int32_t
{
	OFF = 0,
	EASY = 1,    // ±8 seconds
	MEDIUM = 2,  // ±5 seconds
	HARD = 3,    // ±3 seconds
	PRO = 4      // ±1 second
};

// Path and Timing are independent, combinable flags. Running them together is the
// point: routing to a called target while tracking respawns you only heard is the
// dual-task load an actual duel imposes, and practising either one alone does not
// build it. Persisted as an int cvar, so legacy values 0/1/2 keep their meaning and
// 3 means both.
enum class trainer_mode_t : int32_t
{
	OFF = 0,
	PATH = 1,
	TIMING = 2,
	ALL = PATH | TIMING
};

struct map_trainer_item_t
{
	char friendly_name[64];
	char class_name[64];
	vec3_t position;
	edict_t *ent; // cached at build; stable for stock DM items (respawn toggles SVF_RESPAWNING). Rebuilt each map/toggle.
};

// Bodiless simulated duel opponent. It runs a major-item cycle, genuinely consumes
// items via SetRespawn, and is audible when it does - but it has no edict, no
// pathing and never fights. Its health/armor are a resource LEDGER used to drive the
// in-control/out-of-control readout, not a combat model.
// Per-map runtime state: rebuilt every map, never mirrored into map_trainer_config_t.
// Mirrors trainer_config::GHOST_MAX_PATH_POINTS. Declared here because map_trainer_ghost_t
// stores the leg inline and trainer.h must not depend on trainer_config.h; a static_assert
// in trainer_ghost_nav.cpp catches any drift between the two.
constexpr int32_t MAX_GHOST_PATH_POINTS = 96;

struct map_trainer_ghost_t
{
	bool active;
	vec3_t position;    // current interpolated position along the current leg
	vec3_t route_start; // where this leg began
	// Target is held as an edict, NOT an index into map_trainer.items: that array is
	// owned by path mode and is freed when path mode is turned off, which would leave
	// the ghost holding a dangling index. Stock DM item edicts persist across respawn.
	edict_t *target_ent;
	vec3_t target_position;
	// The item the ghost is currently standing on, or null while it is mid-leg. Used to
	// key the pairwise route-distance cache; a mid-leg position has no cache entry.
	edict_t *position_ent;
	gtime_t depart_time;        // left the previous item
	gtime_t arrive_time;        // reaches target_ent
	gtime_t next_pickup_allowed; // hard floor between consecutive pickups
	gtime_t next_decay_time;    // megahealth decay tick

	// The current leg's measured route. leg_count == 0 means we have no nav path and the
	// ghost falls back to a straight line between route_start and target_position.
	// leg_time[] holds the cumulative arrival time at each point, so both the committed
	// arrival and the body's position come from one profile and cannot drift apart.
	vec3_t leg_points[MAX_GHOST_PATH_POINTS];
	float leg_time[MAX_GHOST_PATH_POINTS];
	int32_t leg_count;
	float leg_duration;
	int32_t health;
	int32_t armor;
	int32_t denials_for;     // player beat the ghost to its target
	int32_t denials_against; // ghost took an item the player was timing
	int32_t items_taken;
	int32_t last_control_state; // trainer_logic::control_state_t; only printed on change
	bool control_state_known;

	// Visible silhouette. Purely cosmetic - all gameplay effect comes from the bodiless
	// simulation above. Null when the body level is 0 or no human is in-game yet.
	edict_t *body;
	gtime_t body_frame_time;
};

struct map_trainer_unique_item_t
{
	char class_name[64];
	char friendly_name[64];
	int32_t *item_indices;  // Array of indices into main items array
	int32_t instance_count; // Number of instances of this item type
};

// Rolling record of one practice session on this map. Per-map runtime; reset by
// MapTrainer_Init and reported at match end, on map change, or on demand.
// The single most actionable number here is the MEAN SIGNED error: whether you are
// habitually early or late is a correctable bias, where a mean absolute error is not.
struct map_trainer_session_t
{
	struct item_stat_t
	{
		int32_t id; // trainer_logic::hud_item_id_t
		int32_t attempts;
		float total_signed_error;
		float total_abs_error;
	};

	static constexpr int32_t MAX_ITEM_STATS = 10;

	int32_t attempts;
	int32_t within_window; // only meaningful while a timing challenge mode is active
	int32_t challenge_attempts;
	int32_t early_count;
	int32_t late_count;
	float total_signed_error;
	float total_abs_error;
	float best_abs_error;
	float worst_abs_error;
	item_stat_t items[MAX_ITEM_STATS];
	int32_t item_stat_count;

	// Time-in-control accounting, sampled by the ghost's frame tick.
	gtime_t time_in_control;
	gtime_t time_even;
	gtime_t time_out_of_control;
	gtime_t last_control_sample;
	bool control_sampling;
};

struct map_trainer_t
{
	map_trainer_item_t *items;
	int32_t item_count;
	map_trainer_unique_item_t *unique_items;
	int32_t unique_item_count;
	int32_t current_target_index;
	int32_t previous_target_index;
	bool initialized;
	bool first_pickup;
	bool welcome_message_shown;
	gtime_t welcome_message_time;
	// Practice spawn point for jump training
	vec3_t practice_spawn_origin;
	vec3_t practice_spawn_angles;
	bool practice_spawn_set;
	// Item category toggles
	bool weapons_enabled;
	bool ammo_enabled;
	bool health_enabled;
	bool armor_enabled;
	bool powerups_enabled;
	// Speedometer
	bool speedometer_enabled;
	// Path or Timing trainer (mutually exclusive)
	trainer_mode_t trainer_mode;
	// Combine health packs toggle
	bool combine_health_packs;
	// Path training: include stimpacks / armor shards as targets
	bool path_stimpacks_enabled;
	bool path_armor_shards_enabled;
	// Free collect toggle - allows picking up armor even at max
	bool free_collect_enabled;
	// Debug prints toggle for timing trainer
	bool timing_debug_enabled;
	// Major items only - track only RL/RG/CG + Armors + MH + Quad (default: true for duel practice)
	bool timing_major_items_only;
	// Timing Challenge mode - practice timing items within a difficulty window
	timing_challenge_mode_t timing_challenge_mode;
	// Bhop trainer toggle
	bool bhop_enabled;
	// Spawn trainer
	bool spawn_trainer_enabled;
	edict_t *spawn_trainer_bot;
	edict_t *spawn_trainer_owner;
	bool spawn_trainer_true_random;
	bool spawn_trainer_beacon_enabled;
	gtime_t spawn_trainer_next_beep_time;
	bool spawn_trainer_force_random_pick;
	int32_t spawn_trainer_total_spawns;
	int32_t spawn_trainer_last_spawn_index;
	// Timing trainer data - support for multiple concurrent timings
	struct timing_entry_t {
		bool active;
		gtime_t pickup_time;
		vec3_t position;
		gtime_t respawn_time;
		gtime_t grace_period_end;
		const char *item_name;
		const char *item_classname; // Used as unique identifier
		
		// Source item entity, when known. Lets the evaluator read the engine's real
		// respawn schedule instead of re-deriving it from the pickup time, which
		// matters once the ghost is also consuming items.
		edict_t *item_ent;

		// Megahealth-specific fields
		bool is_megahealth;
		edict_t *megahealth_player; // Player who picked up the megahealth
		bool megahealth_decay_finished; // True when player health <= 100
		gtime_t megahealth_respawn_start; // When the 20-second respawn timer started
	};
	static constexpr int32_t MAX_TIMING_ENTRIES = 32; // Support up to 32 concurrent timings
	timing_entry_t timing_entries[MAX_TIMING_ENTRIES];
	int32_t timing_entry_count;
	// Ghost duel opponent - config toggles (mirrored) plus runtime state (not mirrored)
	bool ghost_enabled;
	bool ghost_timings_enabled; // ghost pickups start timing entries (the by-ear drill)
	int32_t ghost_skill;        // 0 = precise; higher adds arrival jitter
	int32_t ghost_body_level;   // 0 off, 1 line-of-sight, 2 visible through walls
	bool ghost_adaptive;        // routing changes with the ghost's control state
	map_trainer_ghost_t ghost;
	// Ghost Duel HUD (config) and match clock (config length, runtime countdown).
	// Real timing is anchored to a match clock; without one there is nothing to time
	// against. The HUD level is a crutch you dial down as the rhythm internalises.
	int32_t hud_level;        // trainer_logic::hud_level_t
	int32_t match_length_sec; // 0 disables the clock
	gtime_t match_clock_end;
	bool match_clock_running;
	bool match_clock_expired;
	// Feedback discipline: a real duel never tells you your error the instant you make
	// it. Suppressing the per-attempt readout forces self-verification; the session
	// summary is where the learning is consolidated instead.
	bool silent_feedback;
	map_trainer_session_t session;
	// Spawn trainer auto-resume across map changes
	bool spawn_trainer_intent;          // mirrored config: user wants spawn trainer enabled
	bool spawn_trainer_resume_pending;  // runtime: re-spawn bot on this map once a human is in-game
	gtime_t spawn_trainer_resume_next_try; // runtime: backoff between failed resume attempts
};

// Persistent (cross-map) copy of the trainer's CONFIG fields. Lives in game_locals_t,
// which survives map changes; mirrored into/out of level.map_trainer at load/save time.
// In-memory mirror for map changes within a session; archived trainer_* cvars are the
// restart-persistent source of truth (see trainer_cvars.cpp).
// Only configuration is persisted here - never runtime state (item lists, targets, live
// timings, bot edicts), which is rebuilt per map.
struct map_trainer_config_t
{
	bool valid; // false until first save; gates MapTrainer_LoadConfig
	bool weapons_enabled, ammo_enabled, health_enabled, armor_enabled, powerups_enabled;
	bool speedometer_enabled;
	trainer_mode_t trainer_mode;
	bool combine_health_packs;
	bool path_stimpacks_enabled;
	bool path_armor_shards_enabled;
	bool free_collect_enabled;
	bool timing_debug_enabled;
	bool timing_major_items_only;
	timing_challenge_mode_t timing_challenge_mode;
	bool bhop_enabled;
	bool spawn_trainer_true_random;
	bool spawn_trainer_beacon_enabled;
	bool spawn_trainer_intent; // user wants spawn trainer (drives auto-resume)
	bool ghost_enabled;
	bool ghost_timings_enabled;
	int32_t ghost_skill;
	int32_t ghost_body_level;
	bool ghost_adaptive;
	int32_t hud_level;
	int32_t match_length_sec;
	bool silent_feedback;
};

// ==================== CORE API ====================

// Initialization
void MapTrainer_Init();
void MapTrainer_RegisterCvars();

// Config persistence across map changes
void MapTrainer_SaveConfig(); // level.map_trainer -> game.map_trainer_config + archived cvars
void MapTrainer_LoadConfig(); // game.map_trainer_config -> level.map_trainer (in-session overlay)

// Centralized path/timing mode switching. Each flag is enabled and torn down
// independently; enabling one no longer disables the other.
bool MapTrainer_IsModeActive(trainer_mode_t flag);
void MapTrainer_SetModeFlag(trainer_mode_t flag, bool enabled, edict_t *notify = nullptr);
void MapTrainer_ValidateTrainerMode();

// Per-frame trainer tick (deferred spawn-trainer auto-resume)
void MapTrainer_RunFrame();

// ==================== DEBUG API ====================

// Debug logging (controlled by trainer_debug cvar)
void TrainerLog(const char* category, const char* format, ...);
void TrainerLog_Separator();

// ==================== PATH TRAINING API ====================

// Item list building and management
void MapTrainer_BuildItemList(const char *mapname);
void MapTrainer_BuildUniqueItemsList();

// Category and filtering
bool MapTrainer_IsItemCategoryEnabled(const char *class_name);
bool MapTrainer_IsItemCategoryEnabledForItem(const gitem_t *item);
bool MapTrainer_IsCombinableHealthPack(const char *class_name);
const char* MapTrainer_GetNormalizedClassName(const char *class_name);
const char* MapTrainer_GetDisplayFriendlyName(const char *class_name, const char *original_friendly_name);

// Target management
bool MapTrainer_IsItemAvailable(int32_t item_index);
void MapTrainer_PickNewTarget();
bool MapTrainer_IsTargetItem(edict_t *ent);
void MapTrainer_OnItemPickup(edict_t *item_ent, edict_t *player);

// UI
void MapTrainer_ShowWelcomeMessage(edict_t *player);
void MapTrainer_ScheduleWelcomeMessage(edict_t *player);

// ==================== TIMING TRAINER API ====================

// Timing entry management
map_trainer_t::timing_entry_t* MapTrainer_FindTimingEntry(const char *classname, const vec3_t &position);
map_trainer_t::timing_entry_t* MapTrainer_CreateOrUpdateTimingEntry(const char *classname, const char *item_name,
	const vec3_t &position, gtime_t pickup_time, gtime_t respawn_time, edict_t *item_ent = nullptr);

// Duel-relevant item classification and stock respawn schedule. Shared by the timing
// trainer and the ghost so the two can never disagree about when an item comes back.
bool MapTrainer_IsMajorItem(const char *classname);
bool MapTrainer_GetTimedItemInfo(const char *classname, const gitem_t *item,
	const char **out_friendly_name, gtime_t *out_respawn_time);

// An opponent (real bot, or the bodiless ghost) took an item the player was timing.
void MapTrainer_NotifyTimingDenied(map_trainer_t::timing_entry_t *entry, const char *taker);

// When this entry's item actually comes back. Reads the engine's real respawn schedule
// when it is available, so ghost-consumed items are timed against reality.
gtime_t MapTrainer_EntryRespawnTime(const map_trainer_t::timing_entry_t *entry);

// Timing checks
void MapTrainer_OnTimingItemPickup(edict_t *ent, edict_t *other);
void MapTrainer_CheckArmorTiming(edict_t *player);
void MapTrainer_CheckMegahealthTiming(edict_t *player);

// ==================== GHOST DUEL API ====================

void MapTrainer_InitGhost();
void MapTrainer_SetGhostEnabled(bool enabled, edict_t *notify);
void MapTrainer_GhostRunFrame();
bool MapTrainer_IsGhostActive();

// Effective hit points, for the in-control / out-of-control comparison.
int32_t MapTrainer_GhostStack();
int32_t MapTrainer_PlayerStack(const edict_t *player);

// ---- Route measurement (trainer_ghost_nav.cpp) ----

// Rebuild the major-item node table and clear the pairwise distance cache. Call whenever
// the ghost is enabled or the map changes.
void MapTrainer_GhostBuildNodes();
void MapTrainer_GhostResetNav();

// False once the engine has told us this map has no nav data.
bool MapTrainer_GhostNavAvailable();

// True route polyline and its length. Returns false when nav data is unavailable, the
// pair is unreachable, or the route would not fit in max_points (a truncated route
// measures short, so it is rejected rather than under-reported).
bool MapTrainer_GhostQueryRoute(const vec3_t &start, const vec3_t &goal,
	vec3_t *out_points, int32_t max_points, int32_t *out_count, float *out_length);

// Wall- and height-aware straight-line estimate; the fallback when nav is unavailable.
float MapTrainer_GhostEstimateDistance(const vec3_t &start, const vec3_t &goal);

// Distance between two items. measure_now = false returns a cached or estimated value
// cheaply (for scoring many candidates); true pays for a real path query and caches it.
float MapTrainer_GhostRouteDistance(const edict_t *from_ent, const vec3_t &from_pos,
	const edict_t *to_ent, const vec3_t &to_pos, bool measure_now);

// ---- Visible body (trainer_ghost_body.cpp) ----

void MapTrainer_GhostCreateBody();
void MapTrainer_GhostDestroyBody();
void MapTrainer_GhostUpdateBody();

// Measure the current leg and build its time profile. True when a real nav route was used;
// false leaves ghost.leg_count == 0 and callers fall back to a straight line.
bool MapTrainer_GhostBuildLeg(const vec3_t &start, const vec3_t &goal);

// Position along the current leg at `elapsed` seconds since departure.
vec3_t MapTrainer_GhostLegPosition(float elapsed);

// Console diagnostic: reports whether nav data is usable on this map and prints
// straight-line vs real route distance for each pair of majors.
void Cmd_TrainerNavCheck_f(edict_t *ent);

// Called from the pickup path when the player takes an item. Returns true if that
// item was the ghost's current target (i.e. the player denied it), which re-routes
// the ghost.
bool MapTrainer_GhostOnPlayerPickup(edict_t *item_ent, edict_t *player);

// ==================== SESSION SCORING API ====================

void MapTrainer_SessionReset();
void MapTrainer_SessionRecordTiming(const char *classname, float signed_error_sec);
void MapTrainer_SessionSampleControl(int32_t control_state);
void MapTrainer_SessionPauseControlSampling();
void MapTrainer_SessionPrintSummary(edict_t *player);
void MapTrainer_SessionPrintSummaryToAll();
void Cmd_TrainerSession_f(edict_t *ent);

// Shared result reporting for the timing trainer: honours silent mode, records the
// attempt, and prints the per-attempt readout when it is not suppressed.
void MapTrainer_ReportTimingResult(edict_t *player, const char *classname,
	const char *item_name, float time_diff);

// ==================== HUD API (server side) ====================

// Packs the trainer HUD payload into the player's stats. Drawing is in
// trainer_hud_draw.cpp, behind trainer_hud_draw.h.
void MapTrainer_UpdateHudStats(edict_t *player);

void MapTrainer_StartMatchClock();
void MapTrainer_StopMatchClock();
int32_t MapTrainer_MatchClockSecondsLeft();

// ==================== JUMP TRAINER API ====================

// Speedometer
void MapTrainer_UpdateSpeedometer(edict_t *player);

// Bhop consistency tracking
void MapTrainer_UpdateBhopTracking(edict_t *player, pmove_t &pm);

// Practice spawn commands
void Cmd_SetSpawn_f(edict_t *ent);
void Cmd_WarpSpawn_f(edict_t *ent);

// ==================== MENU SYSTEM API ====================

// Main menu
void MapTrainer_OpenMenu(edict_t *ent);
void Cmd_MapTrainerMenu_f(edict_t *ent);

// Menu callbacks
void MapTrainer_MenuClose(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_BackToMainMenu(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_UpdateMenu(edict_t *ent);

// Path training submenu
void MapTrainer_OpenItemPathingSubmenu(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_UpdateItemPathingSubmenu(edict_t *ent);
void MapTrainer_RestartPathTraining();
void MapTrainer_ToggleTraining(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_ToggleWeapons(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_ToggleAmmo(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_ToggleHealth(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_ToggleArmor(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_TogglePowerups(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_ToggleCombineHealthPacks(edict_t *ent, pmenuhnd_t *p);

// Timing trainer submenu
void MapTrainer_OpenItemTimingSubmenu(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_UpdateItemTimingSubmenu(edict_t *ent);
void MapTrainer_ToggleTiming(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_ToggleMajorItemsOnly(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_ToggleFreeCollect(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_ToggleTimingDebug(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_ToggleTimingChallenge(edict_t *ent, pmenuhnd_t *p);

// Ghost duel submenu
void MapTrainer_OpenGhostDuelSubmenu(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_UpdateGhostDuelSubmenu(edict_t *ent);
void MapTrainer_ToggleGhostDuel(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_ToggleGhostTimings(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_CycleGhostSkill(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_CycleHudLevel(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_ToggleSilentFeedback(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_CycleGhostBody(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_ToggleGhostAdaptive(edict_t *ent, pmenuhnd_t *p);

// Jump trainer submenu
void MapTrainer_OpenJumpTrainerSubmenu(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_UpdateJumpTrainerSubmenu(edict_t *ent);
void MapTrainer_ToggleBhopTrainer(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_SavePosition(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_LoadPosition(edict_t *ent, pmenuhnd_t *p);

// Spawn trainer submenu
void MapTrainer_OpenSpawnTrainerSubmenu(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_UpdateSpawnTrainerSubmenu(edict_t *ent);
void MapTrainer_ToggleSpawnTrainer(edict_t *ent, pmenuhnd_t *p);

// Other settings
void MapTrainer_ToggleSpeedometer(edict_t *ent, pmenuhnd_t *p);

// Spawn trainer lifecycle
void MapTrainer_SpawnTrainerRunFrame();
void MapTrainer_InitSpawnTrainerState();
bool MapTrainer_EnableSpawnTrainer(edict_t *requester);
void MapTrainer_DisableSpawnTrainer();
bool MapTrainer_HandleSpawnTrainerBot(edict_t *bot);
void MapTrainer_OnSpawnTrainerClientBegin(edict_t *ent);
void MapTrainer_OnSpawnTrainerClientDisconnect(edict_t *ent);
bool MapTrainer_IsSpawnTrainerBot(const edict_t *ent);
void MapTrainer_OnSpawnTrainerRespawn(edict_t *ent, int32_t total_spawns, int32_t spawn_index);
void MapTrainer_OnSpawnTrainerPlaced(edict_t *ent);

