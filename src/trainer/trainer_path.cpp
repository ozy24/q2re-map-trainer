// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include "trainer_config.h"

// ==================== PATH TRAINING FEATURE ====================

// Check if an item entity is available (not respawning)
bool MapTrainer_IsItemAvailable(const char *class_name, const vec3_t &position)
{
	// Find all entities with matching classname and check if any are available
	edict_t *ent = nullptr;
	while ((ent = G_FindByString<&edict_t::classname>(ent, class_name)) != nullptr)
	{
		vec3_t diff = ent->s.origin - position;
		float distance = diff.length();
		
		// Check if this entity is at the expected position (tight tolerance since items don't move)
		if (distance < trainer_config::PATH_ITEM_POSITION_EPSILON)
		{
			// Check if this item is available (not respawning)
			if (!(ent->svflags & SVF_RESPAWNING) && ent->solid != SOLID_NOT)
			{
				return true; // Found an available instance
			}
		}
	}
	
	return false; // No available instances found
}

void MapTrainer_PickNewTarget()
{
	if (!level.map_trainer.initialized || level.map_trainer.unique_item_count == 0)
		return;
	
	// Get the normalized class name of the previous target to avoid picking the same type
	const char *previous_class_name = nullptr;
	if (level.map_trainer.previous_target_index >= 0 && 
		level.map_trainer.previous_target_index < level.map_trainer.item_count)
	{
		previous_class_name = MapTrainer_GetNormalizedClassName(level.map_trainer.items[level.map_trainer.previous_target_index].class_name);
	}
	
	// Build a list of available unique item types
	std::vector<int32_t> available_unique_types;
	for (int32_t i = 0; i < level.map_trainer.unique_item_count; i++)
	{
		map_trainer_unique_item_t *unique_item = &level.map_trainer.unique_items[i];
		
		// Skip if this is the same type as previous (if we have more than one type)
		if (level.map_trainer.unique_item_count > 1 && previous_class_name != nullptr &&
			Q_strcasecmp(unique_item->class_name, previous_class_name) == 0)
		{
			continue;
		}
		
		// Check if this item category is enabled
		if (!MapTrainer_IsItemCategoryEnabled(unique_item->class_name))
		{
			continue;
		}
		
		// Check if any instance of this item type is available
		bool has_available_instance = false;
		for (int32_t j = 0; j < unique_item->instance_count; j++)
		{
			int32_t item_index = unique_item->item_indices[j];
			map_trainer_item_t *item = &level.map_trainer.items[item_index];
			
			if (MapTrainer_IsItemAvailable(item->class_name, item->position))
			{
				has_available_instance = true;
				break;
			}
		}
		
		if (has_available_instance)
		{
			available_unique_types.push_back(i);
		}
	}
	
	// If no available types found, inform player and wait for respawns
	if (available_unique_types.empty())
	{
		// Send message to all players
		for (uint32_t i = 0; i < game.maxclients; i++)
		{
			edict_t *player = &g_edicts[1 + i];
			if (!player->inuse || !player->client)
				continue;
			gi.LocClient_Print(player, PRINT_CENTER, "Waiting for items to respawn...");
		}
		return;
	}
	
	// Step 1: Pick a random available unique item type (equal weighting for all types)
	int32_t random_index = irandom(static_cast<int32_t>(available_unique_types.size()));
	int32_t unique_type_index = available_unique_types[random_index];
	map_trainer_unique_item_t *unique_item = &level.map_trainer.unique_items[unique_type_index];
	
	// Step 2: Pick a random available instance of that item type
	std::vector<int32_t> available_instances;
	for (int32_t j = 0; j < unique_item->instance_count; j++)
	{
		int32_t item_index = unique_item->item_indices[j];
		map_trainer_item_t *item = &level.map_trainer.items[item_index];
		
		if (MapTrainer_IsItemAvailable(item->class_name, item->position))
		{
			available_instances.push_back(item_index);
		}
	}
	
	if (available_instances.empty())
	{
		return;
	}
	
	int32_t instance_random_index = irandom(static_cast<int32_t>(available_instances.size()));
	int32_t new_target = available_instances[instance_random_index];
	
	// Only update previous_target_index if we have a valid current target
	if (level.map_trainer.current_target_index >= 0)
		level.map_trainer.previous_target_index = level.map_trainer.current_target_index;
	
	level.map_trainer.current_target_index = new_target;
	
	map_trainer_item_t *target = &level.map_trainer.items[level.map_trainer.current_target_index];
	
	// Always show "travel from X to Y" if we have a previous target (and training is enabled)
	if (level.map_trainer.previous_target_index >= 0 && 
		level.map_trainer.previous_target_index < level.map_trainer.item_count &&
		level.map_trainer.trainer_mode == trainer_mode_t::PATH)
	{
		map_trainer_item_t *previous = &level.map_trainer.items[level.map_trainer.previous_target_index];
		
		// Get display-friendly names (handles health pack combining)
		const char *previous_display_name = MapTrainer_GetDisplayFriendlyName(previous->class_name, previous->friendly_name);
		const char *target_display_name = MapTrainer_GetDisplayFriendlyName(target->class_name, target->friendly_name);
		
		// Send message to all players
		for (uint32_t i = 0; i < game.maxclients; i++)
		{
			edict_t *player = &g_edicts[1 + i];
			if (!player->inuse || !player->client)
				continue;
			gi.LocClient_Print(player, PRINT_CENTER, G_Fmt("Travel from {} to {}", previous_display_name, target_display_name).data());
		}
	}
	// Note: No message on first target selection - handled in MapTrainer_OnItemPickup
}

bool MapTrainer_IsTargetItem(edict_t *ent)
{
	if (!level.map_trainer.initialized || level.map_trainer.current_target_index < 0)
		return false;
		
	if (!ent->item || !ent->item->classname)
		return false;
		
	map_trainer_item_t *target = &level.map_trainer.items[level.map_trainer.current_target_index];
	
	// Get normalized class names for comparison (handles health pack combining)
	const char *ent_normalized = MapTrainer_GetNormalizedClassName(ent->item->classname);
	const char *target_normalized = MapTrainer_GetNormalizedClassName(target->class_name);
	
	// Check if normalized class names match
	if (Q_strcasecmp(ent_normalized, target_normalized) == 0)
	{
		return true; // Accept any matching class name (including combined health packs)
	}
	
	return false;
}

void MapTrainer_OnItemPickup(edict_t *item_ent, edict_t *player)
{
	if (!level.map_trainer.initialized)
		return;
	
	const char *item_name = item_ent->item ? item_ent->item->classname : "unknown";
	
	// If this is the first pickup, any item becomes the source
	if (level.map_trainer.first_pickup)
	{
		// Find this item in our CSV data to get its friendly name and store as source
		int32_t source_index = -1;
		for (int32_t i = 0; i < level.map_trainer.item_count; i++)
		{
			if (Q_strcasecmp(level.map_trainer.items[i].class_name, item_name) == 0)
			{
				source_index = i;
				break;
			}
		}
		
		// Mark that we've had our first pickup
		level.map_trainer.first_pickup = false;
		
		// Set the source item BEFORE picking new target
		level.map_trainer.previous_target_index = source_index;
		
		// Pick new target
		MapTrainer_PickNewTarget();
		
		// Show message about the newly selected target
		if (level.map_trainer.current_target_index >= 0 && 
			level.map_trainer.current_target_index < level.map_trainer.item_count)
		{
			map_trainer_item_t *target = &level.map_trainer.items[level.map_trainer.current_target_index];
			const char *target_name = MapTrainer_GetDisplayFriendlyName(target->class_name, target->friendly_name);
			gi.LocClient_Print(player, PRINT_CENTER, G_Fmt("Training started!\nGo to: {}", target_name).data());
		}
	}
	else if (MapTrainer_IsTargetItem(item_ent))
	{
		// Target item picked up!
		// The current target becomes the new source
		level.map_trainer.previous_target_index = level.map_trainer.current_target_index;
		
		// Pick new target
		MapTrainer_PickNewTarget();
	}
}

void MapTrainer_ShowWelcomeMessage(edict_t *player)
{
	if (level.map_trainer.trainer_mode == trainer_mode_t::PATH && level.map_trainer.initialized)
	{
		gi.LocClient_Print(player, PRINT_CENTER, "Map items loaded.\nPlease pick up an item to begin.");
	}
	else if (level.map_trainer.trainer_mode == trainer_mode_t::PATH && !level.map_trainer.initialized)
	{
		gi.LocClient_Print(player, PRINT_CENTER, "No items found in map.\nTraining mode disabled.");
		// Auto-disable training if items weren't found (persist so it doesn't re-arm next map)
		level.map_trainer.trainer_mode = trainer_mode_t::OFF;
		MapTrainer_SaveConfig();
	}
	else
	{
		gi.LocClient_Print(player, PRINT_CENTER, "Welcome to the Q2RE Map Trainer.\nPress Tab to open the training menu.");
	}
}

