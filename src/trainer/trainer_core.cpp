// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include "trainer_config.h"

// ==================== MODE MANAGEMENT ====================

static void MapTrainer_TeardownPathMode()
{
	if (level.map_trainer.items)
	{
		gi.TagFree(level.map_trainer.items);
		level.map_trainer.items = nullptr;
	}
	level.map_trainer.item_count = 0;
	level.map_trainer.initialized = false;
	level.map_trainer.current_target_index = -1;
	level.map_trainer.previous_target_index = -1;
}

static void MapTrainer_TeardownTimingMode()
{
	for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
		level.map_trainer.timing_entries[i].active = false;
	level.map_trainer.timing_entry_count = 0;
}

void MapTrainer_ValidateTrainerMode()
{
	const int32_t mode = static_cast<int32_t>(level.map_trainer.trainer_mode);
	if (mode < static_cast<int32_t>(trainer_mode_t::OFF) ||
		mode > static_cast<int32_t>(trainer_mode_t::TIMING))
	{
		level.map_trainer.trainer_mode = trainer_mode_t::OFF;
	}
}

void MapTrainer_SetMode(trainer_mode_t mode, edict_t *notify)
{
	MapTrainer_ValidateTrainerMode();

	if (mode == level.map_trainer.trainer_mode)
		return;

	const trainer_mode_t old_mode = level.map_trainer.trainer_mode;

	switch (old_mode)
	{
	case trainer_mode_t::PATH:
		MapTrainer_TeardownPathMode();
		break;
	case trainer_mode_t::TIMING:
		MapTrainer_TeardownTimingMode();
		break;
	case trainer_mode_t::OFF:
		break;
	}

	level.map_trainer.trainer_mode = mode;

	switch (mode)
	{
	case trainer_mode_t::PATH:
		if (old_mode == trainer_mode_t::TIMING && notify)
			gi.LocClient_Print(notify, PRINT_HIGH, "Item Timing Trainer automatically disabled.");

		MapTrainer_BuildItemList(level.mapname);
		level.map_trainer.first_pickup = true;
		level.map_trainer.current_target_index = -1;
		level.map_trainer.previous_target_index = -1;

		if (level.map_trainer.initialized)
		{
			if (notify)
				gi.LocClient_Print(notify, PRINT_HIGH, "Item Path Trainer enabled. Items loaded from map.");
		}
		else
		{
			if (notify)
				gi.LocClient_Print(notify, PRINT_HIGH, "No items found in map. Item Path Trainer disabled.");
			level.map_trainer.trainer_mode = trainer_mode_t::OFF;
		}
		break;
	case trainer_mode_t::TIMING:
		if (old_mode == trainer_mode_t::PATH && notify)
			gi.LocClient_Print(notify, PRINT_HIGH, "Item Path Trainer automatically disabled.");
		if (notify)
			gi.LocClient_Print(notify, PRINT_HIGH, "Item Timing Trainer enabled.");
		break;
	case trainer_mode_t::OFF:
		if (notify)
		{
			if (old_mode == trainer_mode_t::PATH)
				gi.LocClient_Print(notify, PRINT_HIGH, "Item Path Trainer disabled.");
			else if (old_mode == trainer_mode_t::TIMING)
				gi.LocClient_Print(notify, PRINT_HIGH, "Item Timing Trainer disabled.");
		}
		break;
	}

	MapTrainer_SaveConfig();
}

// ==================== CORE INITIALIZATION ====================

void MapTrainer_Init()
{
	// Force weapon stay off for training mode
	gi.cvar_set("g_dm_weapons_stay", "0");
	
	level.map_trainer.items = nullptr;
	level.map_trainer.item_count = 0;
	level.map_trainer.current_target_index = -1;
	level.map_trainer.previous_target_index = -1;
	level.map_trainer.initialized = false;
	level.map_trainer.first_pickup = true;
	level.map_trainer.welcome_message_shown = false;
	level.map_trainer.welcome_message_time = 0_ms;
	
	// Initialize all item category toggles to enabled by default
	level.map_trainer.weapons_enabled = true;
	level.map_trainer.ammo_enabled = true;
	level.map_trainer.health_enabled = true;
	level.map_trainer.armor_enabled = true;
	level.map_trainer.powerups_enabled = true;
	
	// Initialize speedometer as enabled by default
	level.map_trainer.speedometer_enabled = true;
	
	// Initialize trainer mode as disabled by default
	level.map_trainer.trainer_mode = trainer_mode_t::OFF;
	// Initialize combine health packs as disabled by default (OFF = separated, ON = combined)
	level.map_trainer.combine_health_packs = false;
	// Initialize bhop trainer as disabled by default
	level.map_trainer.bhop_enabled = false;
	level.map_trainer.spawn_trainer_true_random = false;
	level.map_trainer.spawn_trainer_force_random_pick = false;
	level.map_trainer.spawn_trainer_owner = nullptr;
	level.map_trainer.spawn_trainer_total_spawns = 0;
	level.map_trainer.spawn_trainer_last_spawn_index = -1;
	// Initialize spawn trainer state
	MapTrainer_InitSpawnTrainerState();
	
	// Initialize free collect as enabled by default
	level.map_trainer.free_collect_enabled = true;
	// Initialize debug prints as disabled by default
	level.map_trainer.timing_debug_enabled = false;
	// Initialize major items only as enabled by default (focus on duel-relevant items)
	level.map_trainer.timing_major_items_only = true;
	// Initialize timing challenge as off by default
	level.map_trainer.timing_challenge_mode = timing_challenge_mode_t::OFF;
	// Initialize timing entries array
	level.map_trainer.timing_entry_count = 0;
	for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
	{
		level.map_trainer.timing_entries[i].active = false;
		level.map_trainer.timing_entries[i].pickup_time = 0_ms;
		level.map_trainer.timing_entries[i].position = {};
		level.map_trainer.timing_entries[i].respawn_time = 20_sec;
		level.map_trainer.timing_entries[i].grace_period_end = 0_ms;
		level.map_trainer.timing_entries[i].item_name = nullptr;
		level.map_trainer.timing_entries[i].item_classname = nullptr;
	}

	// Spawn trainer auto-resume defaults (overwritten by LoadConfig below if persisted)
	level.map_trainer.spawn_trainer_intent = false;
	level.map_trainer.spawn_trainer_resume_pending = false;
	level.map_trainer.spawn_trainer_resume_next_try = 0_ms;

	// Overlay any settings the player configured on previous maps
	MapTrainer_LoadConfig();
	MapTrainer_ValidateTrainerMode();

	// Re-establish per-map runtime state from the (possibly restored) config.
	// Item arrays were freed with TAG_LEVEL before this runs and items is now nullptr,
	// so BuildItemList rebuilds cleanly for the new map.
	if (level.map_trainer.trainer_mode == trainer_mode_t::PATH)
	{
		MapTrainer_BuildItemList(level.mapname);
		level.map_trainer.first_pickup = true;
		level.map_trainer.current_target_index = -1;
		level.map_trainer.previous_target_index = -1;
	}

	// Defer spawn-trainer bot re-creation to a server frame (ClientConnect is unsafe
	// during SpawnEntities). MapTrainer_RunFrame() picks this up once a human is in-game.
	level.map_trainer.spawn_trainer_resume_pending = level.map_trainer.spawn_trainer_intent;
}

// ==================== CONFIG PERSISTENCE ====================
// Mirror the trainer's configuration fields between the per-map level.map_trainer and the
// cross-map game.map_trainer_config so settings survive map changes (see plan / trainer.h).

void MapTrainer_SaveConfig()
{
	map_trainer_config_t &cfg = game.map_trainer_config;
	cfg.weapons_enabled = level.map_trainer.weapons_enabled;
	cfg.ammo_enabled = level.map_trainer.ammo_enabled;
	cfg.health_enabled = level.map_trainer.health_enabled;
	cfg.armor_enabled = level.map_trainer.armor_enabled;
	cfg.powerups_enabled = level.map_trainer.powerups_enabled;
	cfg.speedometer_enabled = level.map_trainer.speedometer_enabled;
	cfg.trainer_mode = level.map_trainer.trainer_mode;
	cfg.combine_health_packs = level.map_trainer.combine_health_packs;
	cfg.free_collect_enabled = level.map_trainer.free_collect_enabled;
	cfg.timing_debug_enabled = level.map_trainer.timing_debug_enabled;
	cfg.timing_major_items_only = level.map_trainer.timing_major_items_only;
	cfg.timing_challenge_mode = level.map_trainer.timing_challenge_mode;
	cfg.bhop_enabled = level.map_trainer.bhop_enabled;
	cfg.spawn_trainer_true_random = level.map_trainer.spawn_trainer_true_random;
	cfg.spawn_trainer_beacon_enabled = level.map_trainer.spawn_trainer_beacon_enabled;
	cfg.spawn_trainer_intent = level.map_trainer.spawn_trainer_intent;
	cfg.valid = true;
}

void MapTrainer_LoadConfig()
{
	const map_trainer_config_t &cfg = game.map_trainer_config;
	if (!cfg.valid)
		return; // nothing persisted yet; keep hardcoded defaults

	level.map_trainer.weapons_enabled = cfg.weapons_enabled;
	level.map_trainer.ammo_enabled = cfg.ammo_enabled;
	level.map_trainer.health_enabled = cfg.health_enabled;
	level.map_trainer.armor_enabled = cfg.armor_enabled;
	level.map_trainer.powerups_enabled = cfg.powerups_enabled;
	level.map_trainer.speedometer_enabled = cfg.speedometer_enabled;
	level.map_trainer.trainer_mode = cfg.trainer_mode;
	level.map_trainer.combine_health_packs = cfg.combine_health_packs;
	level.map_trainer.free_collect_enabled = cfg.free_collect_enabled;
	level.map_trainer.timing_debug_enabled = cfg.timing_debug_enabled;
	level.map_trainer.timing_major_items_only = cfg.timing_major_items_only;
	level.map_trainer.timing_challenge_mode = cfg.timing_challenge_mode;
	level.map_trainer.bhop_enabled = cfg.bhop_enabled;
	level.map_trainer.spawn_trainer_true_random = cfg.spawn_trainer_true_random;
	level.map_trainer.spawn_trainer_beacon_enabled = cfg.spawn_trainer_beacon_enabled;
	level.map_trainer.spawn_trainer_intent = cfg.spawn_trainer_intent;
}

// ==================== UTILITY FUNCTIONS ====================

// Helper to convert $item_hyperblaster to Hyperblaster
static void MapTrainer_FriendlyNameFromPickup(const char* pickup_name, char* out, size_t out_size)
{
	if (!pickup_name || !*pickup_name)
	{
		Q_strlcpy(out, "Item", out_size);
		return;
	}
	if (pickup_name[0] == '$')
	{
		// Skip the '$' and 'item_' prefix if present
		const char* name = pickup_name + 1;
		if (strncmp(name, "item_", trainer_config::FRIENDLY_NAME_ITEM_PREFIX_LEN) == 0)
			name += trainer_config::FRIENDLY_NAME_ITEM_PREFIX_LEN;
		// Capitalize first letter, lowercase the rest, and replace underscores with spaces
		if (*name)
		{
			char buf[64];
			Q_strlcpy(buf, name, sizeof(buf));
			buf[0] = toupper(buf[0]);
			for (size_t i = 1; buf[i]; ++i)
			{
				if (buf[i] == '_')
					buf[i] = ' ';
				else
					buf[i] = tolower(buf[i]);
			}
			Q_strlcpy(out, buf, out_size);
			return;
		}
	}
	// Fallback: just copy as is
	Q_strlcpy(out, pickup_name, out_size);
}

// ==================== CATEGORY MANAGEMENT ====================

bool MapTrainer_IsCombinableHealthPack(const char *class_name)
{
	// These health packs can be combined when the option is enabled
	// Mega health is NOT included in this list
	return (Q_strcasecmp(class_name, "item_health_small") == 0 ||
			Q_strcasecmp(class_name, "item_health") == 0 ||
			Q_strcasecmp(class_name, "item_health_large") == 0);
}

const char* MapTrainer_GetNormalizedClassName(const char *class_name)
{
	// If combine health packs is enabled, normalize health pack class names (combine them)
	if (level.map_trainer.combine_health_packs && MapTrainer_IsCombinableHealthPack(class_name))
	{
		return "item_health_combined"; // Virtual class name for combined health packs
	}
	return class_name;
}

const char* MapTrainer_GetDisplayFriendlyName(const char *class_name, const char *original_friendly_name)
{
	// If combine health packs is enabled, use generic name for combinable health packs
	if (level.map_trainer.combine_health_packs && MapTrainer_IsCombinableHealthPack(class_name))
	{
		return "Health Pack"; // Generic display name for combined health packs
	}
	return original_friendly_name;
}

// Determine what category an item belongs to based on its class name
bool MapTrainer_IsItemCategoryEnabled(const char *class_name)
{
	// Weapons
	if (strstr(class_name, "weapon_") == class_name ||
		Q_strcasecmp(class_name, "item_quad") == 0) // Quad damage is a weapon powerup but treat as weapon
	{
		return level.map_trainer.weapons_enabled;
	}
	
	// Ammo
	if (strstr(class_name, "ammo_") == class_name)
	{
		return level.map_trainer.ammo_enabled;
	}
	
	// Health items (including combined health pack virtual class)
	if (strstr(class_name, "item_health") == class_name ||
		Q_strcasecmp(class_name, "item_health_small") == 0 ||
		Q_strcasecmp(class_name, "item_health_large") == 0 ||
		Q_strcasecmp(class_name, "item_health_mega") == 0 ||
		Q_strcasecmp(class_name, "item_health_combined") == 0)
	{
		return level.map_trainer.health_enabled;
	}
	
	// Armor items
	if (strstr(class_name, "item_armor") == class_name ||
		Q_strcasecmp(class_name, "item_jacket_armor") == 0 ||
		Q_strcasecmp(class_name, "item_combat_armor") == 0 ||
		Q_strcasecmp(class_name, "item_body_armor") == 0 ||
		Q_strcasecmp(class_name, "item_power_screen") == 0 ||
		Q_strcasecmp(class_name, "item_power_shield") == 0)
	{
		return level.map_trainer.armor_enabled;
	}
	
	// Powerups (everything else)
	if (strstr(class_name, "item_") == class_name)
	{
		return level.map_trainer.powerups_enabled;
	}
	
	// Default to enabled for unknown items
	return true;
}

// ==================== ITEM LIST BUILDING ====================

void MapTrainer_BuildItemList(const char *mapname)
{
	// Clean up previous data (using TagFree for TAG_LEVEL allocated memory)
	if (level.map_trainer.items)
	{
		gi.TagFree(level.map_trainer.items);
		level.map_trainer.items = nullptr;
	}

	std::vector<map_trainer_item_t> temp_items;

	// Enumerate all in-game entities (skip world and clients)
	for (uint32_t i = 1; i < globals.num_edicts; i++)
	{
		edict_t *ent = &g_edicts[i];
		if (!ent->inuse || !ent->classname || !ent->item)
			continue;

		// Skip entities with invalid classnames
		if (Q_strcasecmp(ent->classname, "noclass") == 0 || Q_strcasecmp(ent->classname, "freed") == 0)
			continue;

		// Only include actual items (entities with an item field)
		// Only include items that are in the enabled categories
		if (!MapTrainer_IsItemCategoryEnabled(ent->classname))
			continue;

		// Get friendly name from the actual item definition that the game uses
		char friendly_name[64];
		if (ent->item && ent->item->pickup_name)
			MapTrainer_FriendlyNameFromPickup(ent->item->pickup_name, friendly_name, sizeof(friendly_name));
		else
			Q_strlcpy(friendly_name, ent->classname, sizeof(friendly_name));

		map_trainer_item_t item;
		Q_strlcpy(item.friendly_name, friendly_name, sizeof(item.friendly_name));
		Q_strlcpy(item.class_name, ent->classname, sizeof(item.class_name));
		item.position = ent->s.origin;

		temp_items.push_back(item);
	}

	level.map_trainer.item_count = static_cast<int32_t>(temp_items.size());
	if (level.map_trainer.item_count > 0)
	{
		// Use TagMalloc for automatic cleanup on level change
		level.map_trainer.items = (map_trainer_item_t*)gi.TagMalloc(
			sizeof(map_trainer_item_t) * level.map_trainer.item_count, TAG_LEVEL);
		for (size_t i = 0; i < temp_items.size(); i++)
		{
			level.map_trainer.items[i] = temp_items[i];
		}
		level.map_trainer.initialized = true;
		MapTrainer_BuildUniqueItemsList();
	}
	else
	{
		level.map_trainer.initialized = false;
	}
}

void MapTrainer_BuildUniqueItemsList()
{
	if (!level.map_trainer.initialized || level.map_trainer.item_count == 0)
		return;
	
	// Allocate memory for unique items (worst case: all items are unique)
	level.map_trainer.unique_items = (map_trainer_unique_item_t*)gi.TagMalloc(
		sizeof(map_trainer_unique_item_t) * level.map_trainer.item_count, TAG_LEVEL);
	level.map_trainer.unique_item_count = 0;
	
	// Process each item to build unique list
	for (size_t i = 0; i < static_cast<size_t>(level.map_trainer.item_count); i++)
	{
		map_trainer_item_t *current_item = &level.map_trainer.items[i];
		bool found_existing = false;
		
		// Get normalized class name (handles health pack combining)
		const char *normalized_class_name = MapTrainer_GetNormalizedClassName(current_item->class_name);
		
		// Check if this normalized class_name already exists in unique list
		for (size_t j = 0; j < static_cast<size_t>(level.map_trainer.unique_item_count); j++)
		{
			if (Q_strcasecmp(level.map_trainer.unique_items[j].class_name, normalized_class_name) == 0)
			{
				// Found existing unique item, add this instance to it
				map_trainer_unique_item_t *unique_item = &level.map_trainer.unique_items[j];
				
				// Reallocate indices array to fit one more
				int32_t *new_indices = (int32_t*)gi.TagMalloc(
					sizeof(int32_t) * (unique_item->instance_count + 1), TAG_LEVEL);
				
				// Copy existing indices
				if (unique_item->item_indices)
				{
					memcpy(new_indices, unique_item->item_indices, sizeof(int32_t) * unique_item->instance_count);
					gi.TagFree(unique_item->item_indices);
				}
				
				// Add new index
				new_indices[unique_item->instance_count] = static_cast<int32_t>(i);
				unique_item->item_indices = new_indices;
				unique_item->instance_count++;
				
				found_existing = true;
				break;
			}
		}
		
		if (!found_existing)
		{
			// Create new unique item entry
			map_trainer_unique_item_t *unique_item = &level.map_trainer.unique_items[level.map_trainer.unique_item_count];
			
			Q_strlcpy(unique_item->class_name, normalized_class_name, sizeof(unique_item->class_name));
			// For combined health packs, use a generic friendly name
			if (level.map_trainer.combine_health_packs && MapTrainer_IsCombinableHealthPack(current_item->class_name))
			{
				Q_strlcpy(unique_item->friendly_name, "health pack", sizeof(unique_item->friendly_name));
			}
			else
			{
				Q_strlcpy(unique_item->friendly_name, current_item->friendly_name, sizeof(unique_item->friendly_name));
			}
			
			// Allocate indices array with first instance
			unique_item->item_indices = (int32_t*)gi.TagMalloc(sizeof(int32_t), TAG_LEVEL);
			unique_item->item_indices[0] = static_cast<int32_t>(i);
			unique_item->instance_count = 1;
			
			level.map_trainer.unique_item_count++;
		}
	}
	
	// Debug output removed to reduce verbosity
}

