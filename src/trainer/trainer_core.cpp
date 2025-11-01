// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"

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
	
	// Initialize training as disabled by default
	level.map_trainer.training_enabled = false;
	
	// Initialize combine health packs as disabled by default (OFF = separated, ON = combined)
	level.map_trainer.combine_health_packs = false;
	
	// Initialize timing trainer as disabled by default
	level.map_trainer.timing_enabled = false;
	// Initialize bhop trainer as disabled by default
	level.map_trainer.bhop_enabled = false;
	
	// Initialize free collect as enabled by default
	level.map_trainer.free_collect_enabled = true;
	// Initialize debug prints as disabled by default
	level.map_trainer.timing_debug_enabled = false;
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
		if (strncmp(name, "item_", 5) == 0)
			name += 5;
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

