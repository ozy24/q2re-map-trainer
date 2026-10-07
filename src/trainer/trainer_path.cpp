// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include "trainer_config.h"
#include "trainer_logic.h"

#include <memory>

// ==================== PATH TRAINING FEATURE ====================

bool MapTrainer_IsItemAvailable(int32_t item_index)
{
	if (!level.map_trainer.items || item_index < 0 || item_index >= level.map_trainer.item_count)
		return false;

	edict_t *ent = level.map_trainer.items[item_index].ent;
	// Stock items reuse the same edict slot across respawns; list is rebuilt if entities are freed.
	if (!ent || !ent->inuse)
		return false;

	return !(ent->svflags & SVF_RESPAWNING) && ent->solid != SOLID_NOT;
}

// Item Run: an item collected this run is never a target again until the run ends, even
// if something brought it back early.
static bool MapTrainer_IsItemTargetable(int32_t item_index)
{
	if (!MapTrainer_IsItemAvailable(item_index))
		return false;

	return !(level.map_trainer.path_run_mode && level.map_trainer.items[item_index].collected_in_run);
}

static int32_t MapTrainer_RunItemsRemaining()
{
	if (!level.map_trainer.items || level.map_trainer.item_count <= 0)
		return 0;

	auto collected = std::make_unique<bool[]>(level.map_trainer.item_count);
	for (int32_t i = 0; i < level.map_trainer.item_count; i++)
		collected[i] = level.map_trainer.items[i].collected_in_run;
	return trainer_logic::RunItemsRemaining(collected.get(), level.map_trainer.item_count);
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
	auto collect_types = [&](bool allow_repeat_type)
	{
		for (int32_t i = 0; i < level.map_trainer.unique_item_count; i++)
		{
			map_trainer_unique_item_t *unique_item = &level.map_trainer.unique_items[i];
			
			// Skip if this is the same type as previous (if we have more than one type)
			if (!allow_repeat_type && trainer_logic::ShouldSkipUniqueTypeForPick(level.map_trainer.unique_item_count,
				unique_item->class_name, previous_class_name))
			{
				continue;
			}
			
			// Category toggles are applied at BuildItemList time (rebuilt on every toggle).
			// Check if any instance of this item type is available
			bool has_available_instance = false;
			for (int32_t j = 0; j < unique_item->instance_count; j++)
			{
				const int32_t item_index = unique_item->item_indices[j];
				if (MapTrainer_IsItemTargetable(item_index))
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
	};
	collect_types(false);

	// Item Run: near the end of a run the only items left can be the type just taken
	// (the last two shards). Nothing respawns to break the tie, so allow the repeat.
	if (available_unique_types.empty() && level.map_trainer.path_run_mode)
		collect_types(true);
	
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
		const int32_t item_index = unique_item->item_indices[j];
		if (MapTrainer_IsItemTargetable(item_index))
			available_instances.push_back(item_index);
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
		MapTrainer_IsModeActive(trainer_mode_t::PATH))
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
			if (level.map_trainer.path_run_mode)
				gi.LocClient_Print(player, PRINT_CENTER, G_Fmt("Travel from {} to {}\n{} left", previous_display_name,
					target_display_name, MapTrainer_RunItemsRemaining()).data());
			else
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
	
	// Prefer exact entity match when the cached edict is still valid
	if (target->ent && target->ent->inuse && ent == target->ent)
		return true;

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

// ==================== ITEM RUN ====================

static void MapTrainer_PrintCenterToAll(const char *message)
{
	for (uint32_t i = 0; i < game.maxclients; i++)
	{
		edict_t *player = &g_edicts[1 + i];
		if (!player->inuse || !player->client)
			continue;
		gi.LocClient_Print(player, PRINT_CENTER, message);
	}
}

static void MapTrainer_MarkRunCollected(edict_t *item_ent)
{
	if (!level.map_trainer.path_run_mode || !level.map_trainer.items)
		return;

	for (int32_t i = 0; i < level.map_trainer.item_count; i++)
	{
		if (level.map_trainer.items[i].ent == item_ent)
		{
			level.map_trainer.items[i].collected_in_run = true;
			return;
		}
	}
}

// Every item is collected: report the time, then hold everything for a moment so the
// result can be read before MapTrainer_PathRunFrame brings the items back. No target is
// set and first_pickup stays false meanwhile, so the pickup gate in Touch_Item blocks
// everything until the release.
static void MapTrainer_CompleteRun()
{
	const int64_t run_ms = (level.time - level.map_trainer.run_start_time).milliseconds();
	const bool new_best = trainer_logic::IsNewBestRun(run_ms, level.map_trainer.run_best_time.milliseconds());
	if (new_best)
		level.map_trainer.run_best_time = level.time - level.map_trainer.run_start_time;

	char run_time[32];
	char best_time[32];
	trainer_logic::FormatRunTime(run_ms, run_time, sizeof(run_time));
	trainer_logic::FormatRunTime(level.map_trainer.run_best_time.milliseconds(), best_time, sizeof(best_time));

	char best_line[64];
	if (new_best)
		Q_strlcpy(best_line, "New best!", sizeof(best_line));
	else
		G_FmtTo(best_line, "Best: {}", best_time);

	// Save the run. Only runs over the full item set are comparable, so only those rank.
	const trainer_logic::category_toggles_t toggles = {
		level.map_trainer.weapons_enabled, level.map_trainer.ammo_enabled, level.map_trainer.health_enabled,
		level.map_trainer.armor_enabled, level.map_trainer.powerups_enabled };
	const bool ranked = trainer_logic::IsRunRanked(toggles, level.map_trainer.path_stimpacks_enabled,
		level.map_trainer.path_armor_shards_enabled);
	const int32_t rank = MapTrainer_RecordsAddRun(run_ms, level.map_trainer.item_count, ranked);

	char record_time[32];
	trainer_logic::FormatRunTime(MapTrainer_RecordsBestMs(), record_time, sizeof(record_time));
	char result_lines[128];
	char console_rank[48];
	if (!ranked)
	{
		// Unranked runs still get the session best on this map.
		G_FmtTo(result_lines, "Unranked: turn every item type on to rank\n{}", best_line);
		Q_strlcpy(console_rank, "unranked", sizeof(console_rank));
	}
	else if (rank == 1)
	{
		Q_strlcpy(result_lines, "New map record!", sizeof(result_lines));
		Q_strlcpy(console_rank, "ranked #1, map record", sizeof(console_rank));
	}
	else if (rank > 0)
	{
		G_FmtTo(result_lines, "#{} on {} (record {})", rank, level.mapname, record_time);
		G_FmtTo(console_rank, "ranked #{}", rank);
	}
	else
	{
		G_FmtTo(result_lines, "Record: {}", record_time);
		G_FmtTo(console_rank, "outside the top {}", trainer_config::RUN_RECORDS_MAX);
	}

	MapTrainer_PrintCenterToAll(G_Fmt("Run {} complete!\n{} items in {}\n{}\nItems respawn in {:.0f} seconds",
		level.map_trainer.run_number, level.map_trainer.item_count, run_time, result_lines,
		trainer_config::ITEM_RUN_RESPAWN_DELAY_SEC).data());

	// A console line too, so a session's runs can be compared afterwards.
	for (uint32_t i = 0; i < game.maxclients; i++)
	{
		edict_t *player = &g_edicts[1 + i];
		if (!player->inuse || !player->client)
			continue;
		gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("Item run {} on {}: {} items in {} - {}",
			level.map_trainer.run_number, level.mapname, level.map_trainer.item_count, run_time,
			console_rank).data());
	}
	TrainerLog("RUN", "run %d complete: %d items in %s (%s)", level.map_trainer.run_number,
		level.map_trainer.item_count, run_time, console_rank);

	level.map_trainer.current_target_index = -1;
	level.map_trainer.previous_target_index = -1;
	level.map_trainer.first_pickup = false;
	level.map_trainer.run_release_time = level.time + gtime_t::from_sec(trainer_config::ITEM_RUN_RESPAWN_DELAY_SEC);
}

void MapTrainer_ReleaseRunItems()
{
	if (!level.map_trainer.items)
		return;

	for (int32_t i = 0; i < level.map_trainer.item_count; i++)
	{
		map_trainer_item_t *item = &level.map_trainer.items[i];
		if (!item->collected_in_run)
			continue;
		item->collected_in_run = false;

		edict_t *ent = item->ent;
		if (!ent || !ent->inuse)
			continue;

		if (ent->svflags & SVF_RESPAWNING)
		{
			// Held by MapTrainer_PathRunFrame: let the stock DoRespawn run now.
			ent->nextthink = level.time;
		}
		else if ((ent->svflags & SVF_NOCLIENT) && ent->item && (ent->item->tag & HEALTH_TIMED))
		{
			// A megahealth still waiting for its owner's health to drain (MegaHealth_think)
			// has not scheduled its respawn yet. Schedule it for now.
			SetRespawn(ent, 0_ms);
		}
	}
}

void MapTrainer_PathRunFrame()
{
	if (!level.map_trainer.initialized || !level.map_trainer.items || !MapTrainer_IsModeActive(trainer_mode_t::PATH))
		return;

	// A finished run's pause is over: bring everything back and wait for the next first pickup.
	if (level.map_trainer.run_release_time > 0_ms && level.time >= level.map_trainer.run_release_time)
	{
		level.map_trainer.run_release_time = 0_ms;
		MapTrainer_ReleaseRunItems();
		level.map_trainer.first_pickup = true;
		MapTrainer_PrintCenterToAll(G_Fmt("Items are back.\nPick up any item to start run {}.",
			level.map_trainer.run_number + 1).data());
		return;
	}

	if (!level.map_trainer.path_run_mode)
		return;

	// Hold every collected item's respawn. Pushing nextthink a second ahead each frame,
	// rather than parking it far away, means anything that stops calling this (run mode
	// off, path mode off, a rebuilt item list) lets the items come back on their own
	// within a second. A megahealth reaches this state once its owner's bonus health has
	// drained and MegaHealth_think calls SetRespawn.
	const gtime_t hold_until = level.time + 1_sec;
	for (int32_t i = 0; i < level.map_trainer.item_count; i++)
	{
		const map_trainer_item_t *item = &level.map_trainer.items[i];
		edict_t *ent = item->ent;
		if (!item->collected_in_run || !ent || !ent->inuse || !(ent->svflags & SVF_RESPAWNING))
			continue;
		if (ent->nextthink < hold_until)
			ent->nextthink = hold_until;
	}
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

		// Item Run: the first pickup starts the clock and is the run's first item.
		if (level.map_trainer.path_run_mode)
		{
			level.map_trainer.run_number++;
			level.map_trainer.run_start_time = level.time;
			MapTrainer_MarkRunCollected(item_ent);
			if (MapTrainer_RunItemsRemaining() == 0)
			{
				MapTrainer_CompleteRun();
				return;
			}
		}
		
		// Pick new target
		MapTrainer_PickNewTarget();
		
		// Show message about the newly selected target
		if (level.map_trainer.current_target_index >= 0 && 
			level.map_trainer.current_target_index < level.map_trainer.item_count)
		{
			map_trainer_item_t *target = &level.map_trainer.items[level.map_trainer.current_target_index];
			const char *target_name = MapTrainer_GetDisplayFriendlyName(target->class_name, target->friendly_name);
			if (level.map_trainer.path_run_mode)
				gi.LocClient_Print(player, PRINT_CENTER, G_Fmt("Run {} started - {} items\nGo to: {}",
					level.map_trainer.run_number, level.map_trainer.item_count, target_name).data());
			else
				gi.LocClient_Print(player, PRINT_CENTER, G_Fmt("Training started!\nGo to: {}", target_name).data());
		}
	}
	else if (MapTrainer_IsTargetItem(item_ent))
	{
		// Target item picked up!
		// The current target becomes the new source
		level.map_trainer.previous_target_index = level.map_trainer.current_target_index;

		if (level.map_trainer.path_run_mode)
		{
			MapTrainer_MarkRunCollected(item_ent);
			if (MapTrainer_RunItemsRemaining() == 0)
			{
				MapTrainer_CompleteRun();
				return;
			}
		}
		
		// Pick new target
		MapTrainer_PickNewTarget();
	}
}

void MapTrainer_ShowWelcomeMessage(edict_t *player)
{
	if (MapTrainer_IsModeActive(trainer_mode_t::PATH) && level.map_trainer.initialized)
	{
		if (level.map_trainer.path_run_mode)
			gi.LocClient_Print(player, PRINT_CENTER, "Item Run: items stay gone until you have them all.\nPick up an item to begin.");
		else
			gi.LocClient_Print(player, PRINT_CENTER, "Map items loaded.\nPlease pick up an item to begin.");
	}
	else if (MapTrainer_IsModeActive(trainer_mode_t::PATH) && !level.map_trainer.initialized)
	{
		gi.LocClient_Print(player, PRINT_CENTER, "No items found in map.\nPath trainer disabled.");
		// Auto-disable path training if items weren't found (persist so it doesn't re-arm
		// next map). Trainers are exclusive, so nothing else was running alongside it.
		MapTrainer_SetModeFlag(trainer_mode_t::PATH, false);
	}
	else
	{
		gi.LocClient_Print(player, PRINT_CENTER, "Welcome to the Q2RE Map Trainer.\nType maptrainer in the console to open the menu.");
	}
}

void MapTrainer_ScheduleWelcomeMessage(edict_t *player)
{
	if (!player || !player->client || (player->svflags & SVF_BOT))
		return;

	const bool path_awaiting_pickup = MapTrainer_IsModeActive(trainer_mode_t::PATH) &&
		level.map_trainer.initialized && level.map_trainer.first_pickup;

	if (level.map_trainer.welcome_message_shown && !path_awaiting_pickup)
		return;

	level.map_trainer.welcome_message_shown = false;
	level.map_trainer.welcome_message_time = level.time + 500_ms;
}
