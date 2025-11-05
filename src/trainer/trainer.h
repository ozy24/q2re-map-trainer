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

// ==================== DATA STRUCTURES ====================

struct map_trainer_item_t
{
	char friendly_name[64];
	char class_name[64];
	vec3_t position;
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
	// Training mode toggle
	bool training_enabled;
	// Combine health packs toggle
	bool combine_health_packs;
	// Timing trainer toggle
	bool timing_enabled;
	// Free collect toggle - allows picking up armor even at max
	bool free_collect_enabled;
	// Debug prints toggle for timing trainer
	bool timing_debug_enabled;
	// Major items only - track only RL/RG/CG + Armors + MH + Quad (default: true for duel practice)
	bool timing_major_items_only;
	// Bhop trainer toggle
	bool bhop_enabled;
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
};

// ==================== CORE API ====================

// Initialization
void MapTrainer_Init();

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
bool MapTrainer_IsCombinableHealthPack(const char *class_name);
const char* MapTrainer_GetNormalizedClassName(const char *class_name);
const char* MapTrainer_GetDisplayFriendlyName(const char *class_name, const char *original_friendly_name);

// Target management
bool MapTrainer_IsItemAvailable(const char *class_name, const vec3_t &position);
void MapTrainer_PickNewTarget();
bool MapTrainer_IsTargetItem(edict_t *ent);
void MapTrainer_OnItemPickup(edict_t *item_ent, edict_t *player);

// UI
void MapTrainer_ShowWelcomeMessage(edict_t *player);

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

// Jump trainer submenu
void MapTrainer_OpenJumpTrainerSubmenu(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_UpdateJumpTrainerSubmenu(edict_t *ent);
void MapTrainer_ToggleBhopTrainer(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_SavePosition(edict_t *ent, pmenuhnd_t *p);
void MapTrainer_LoadPosition(edict_t *ent, pmenuhnd_t *p);

// Other settings
void MapTrainer_ToggleSpeedometer(edict_t *ent, pmenuhnd_t *p);

