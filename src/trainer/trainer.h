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

// Path and Timing trainers are mutually exclusive
enum class trainer_mode_t : int32_t
{
	OFF = 0,
	PATH = 1,
	TIMING = 2
};

struct map_trainer_item_t
{
	char friendly_name[64];
	char class_name[64];
	vec3_t position;
	edict_t *ent; // cached at build; stable for stock DM items (respawn toggles SVF_RESPAWNING). Rebuilt each map/toggle.
};

struct map_trainer_unique_item_t
{
	char class_name[64];
	char friendly_name[64];
	int32_t *item_indices;  // Array of indices into main items array
	int32_t instance_count; // Number of instances of this item type
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
		
		// Megahealth-specific fields
		bool is_megahealth;
		edict_t *megahealth_player; // Player who picked up the megahealth
		bool megahealth_decay_finished; // True when player health <= 100
		gtime_t megahealth_respawn_start; // When the 20-second respawn timer started
	};
	static constexpr int32_t MAX_TIMING_ENTRIES = 32; // Support up to 32 concurrent timings
	timing_entry_t timing_entries[MAX_TIMING_ENTRIES];
	int32_t timing_entry_count;
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
	bool free_collect_enabled;
	bool timing_debug_enabled;
	bool timing_major_items_only;
	timing_challenge_mode_t timing_challenge_mode;
	bool bhop_enabled;
	bool spawn_trainer_true_random;
	bool spawn_trainer_beacon_enabled;
	bool spawn_trainer_intent; // user wants spawn trainer (drives auto-resume)
};

// ==================== CORE API ====================

// Initialization
void MapTrainer_Init();
void MapTrainer_RegisterCvars();

// Config persistence across map changes
void MapTrainer_SaveConfig(); // level.map_trainer -> game.map_trainer_config + archived cvars
void MapTrainer_LoadConfig(); // game.map_trainer_config -> level.map_trainer (in-session overlay)

// Centralized path/timing mode switching (mutual exclusion)
void MapTrainer_SetMode(trainer_mode_t mode, edict_t *notify = nullptr);
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
	const vec3_t &position, gtime_t pickup_time, gtime_t respawn_time);

// Timing checks
void MapTrainer_OnTimingItemPickup(edict_t *ent, edict_t *other);
void MapTrainer_CheckArmorTiming(edict_t *player);
void MapTrainer_CheckMegahealthTiming(edict_t *player);

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
void MapTrainer_InitSpawnTrainerState();
bool MapTrainer_EnableSpawnTrainer(edict_t *requester);
void MapTrainer_DisableSpawnTrainer();
bool MapTrainer_HandleSpawnTrainerBot(edict_t *bot);
void MapTrainer_OnSpawnTrainerClientBegin(edict_t *ent);
void MapTrainer_OnSpawnTrainerClientDisconnect(edict_t *ent);
bool MapTrainer_IsSpawnTrainerBot(const edict_t *ent);
void MapTrainer_OnSpawnTrainerRespawn(edict_t *ent, int32_t total_spawns, int32_t spawn_index);
void MapTrainer_OnSpawnTrainerPlaced(edict_t *ent);

