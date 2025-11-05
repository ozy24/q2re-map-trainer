// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"

// ==================== TIMING TRAINER FEATURE ====================

// Constants
constexpr float ITEM_PICKUP_RADIUS = 64.0f; // Standard Quake 2 pickup radius
constexpr float ITEM_POSITION_EPSILON = 16.0f; // Distance threshold for considering items at same position
constexpr gtime_t MEGAHEALTH_INDEFINITE_GRACE = 999999_sec; // Grace period until decay finishes
constexpr gtime_t MEGAHEALTH_PHASE2_GRACE = 3_sec; // Grace period after decay before timing checks
constexpr gtime_t MEGAHEALTH_EXPIRE_TIMEOUT = 60_sec; // Auto-expire megahealth entry if player doesn't return

// Helper function to check if item is "major" for duel practice
// Major items: RL/RG/CG (trifecta) + All Armors + Megahealth + Quad
bool MapTrainer_IsMajorItem(const char* classname)
{
	if (!classname)
		return false;
	
	// All armors are major items in duels
	if (Q_strcasecmp(classname, "item_armor_body") == 0)       // Red Armor
		return true;
	if (Q_strcasecmp(classname, "item_armor_combat") == 0)     // Yellow Armor
		return true;
	if (Q_strcasecmp(classname, "item_armor_jacket") == 0)     // Green Armor
		return true;
	
	// Megahealth is critical
	if (Q_strcasecmp(classname, "item_health_mega") == 0)
		return true;
	
	// Quad is critical
	if (Q_strcasecmp(classname, "item_quad") == 0)
		return true;
	
	// Main weapons only (Trifecta)
	if (Q_strcasecmp(classname, "weapon_railgun") == 0)
		return true;
	if (Q_strcasecmp(classname, "weapon_rocketlauncher") == 0)
		return true;
	if (Q_strcasecmp(classname, "weapon_chaingun") == 0)
		return true;
	
	// Everything else is filtered out (shotgun, hyperblaster, BFG, etc.)
	return false;
}

// Helper function to find existing timing entry for an item
// Uses both classname AND position to differentiate multiple items of the same type
map_trainer_t::timing_entry_t* MapTrainer_FindTimingEntry(const char *classname, const vec3_t &position)
{
	if (!classname)
		return nullptr;
		
	for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
	{
		if (level.map_trainer.timing_entries[i].active && 
			level.map_trainer.timing_entries[i].item_classname &&
			Q_strcasecmp(level.map_trainer.timing_entries[i].item_classname, classname) == 0)
		{
			// Check if position matches (within epsilon tolerance)
			vec3_t diff = position - level.map_trainer.timing_entries[i].position;
			float distance = diff.length();
			if (distance <= ITEM_POSITION_EPSILON)
			{
				return &level.map_trainer.timing_entries[i];
			}
		}
	}
	return nullptr;
}

// Helper function to create or update timing entry for an item
map_trainer_t::timing_entry_t* MapTrainer_CreateOrUpdateTimingEntry(const char *classname, const char *item_name, 
	const vec3_t &position, gtime_t pickup_time, gtime_t respawn_time)
{
	if (!classname || !item_name)
		return nullptr;
		
	// First try to find existing entry (by classname AND position)
	map_trainer_t::timing_entry_t* entry = MapTrainer_FindTimingEntry(classname, position);
	
	// If not found, create new entry
	if (!entry)
	{
		// Find first inactive slot
		for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
		{
			if (!level.map_trainer.timing_entries[i].active)
			{
				entry = &level.map_trainer.timing_entries[i];
				// Increment count with bounds check to prevent overflow
				if (level.map_trainer.timing_entry_count < level.map_trainer.MAX_TIMING_ENTRIES)
					level.map_trainer.timing_entry_count++;
				break;
			}
		}
		
		// If no free slots, return nullptr (could implement LRU replacement later)
		if (!entry)
			return nullptr;
	}
	
	// Update entry data
	entry->active = true;
	entry->pickup_time = pickup_time;
	entry->position = position;
	entry->respawn_time = respawn_time;
	entry->grace_period_end = pickup_time + 5_sec;
	entry->item_name = item_name;
	entry->item_classname = classname;
	
	// Initialize megahealth fields to default values
	entry->is_megahealth = false;
	entry->megahealth_player = nullptr;
	entry->megahealth_decay_finished = false;
	entry->megahealth_respawn_start = 0_ms;
	
	return entry;
}

// [Map Trainer] New function - handles timing trainer item pickup logic
void MapTrainer_OnTimingItemPickup(edict_t *ent, edict_t *other)
{
	if (!level.map_trainer.timing_enabled || !ent->item || !ent->item->classname)
		return;

	const char *classname = ent->item->classname;
	
	// Check major items filter
	if (level.map_trainer.timing_major_items_only && !MapTrainer_IsMajorItem(classname))
		return;  // Skip this item if filter is enabled and item is not major
	const char *item_name = nullptr;
	gtime_t respawn_time = 20_sec; // Default respawn time
	
	// Check for armor items (20 second respawn)
	if (Q_strcasecmp(classname, "item_armor_jacket") == 0)
	{
		item_name = "Green Armor";
		respawn_time = 20_sec;
	}
	else if (Q_strcasecmp(classname, "item_armor_combat") == 0)
	{
		item_name = "Yellow Armor";
		respawn_time = 20_sec;
	}
	else if (Q_strcasecmp(classname, "item_armor_body") == 0)
	{
		item_name = "Red Armor";
		respawn_time = 20_sec;
	}
	// Check for power armor items (20 second respawn)
	else if (Q_strcasecmp(classname, "item_power_screen") == 0)
	{
		item_name = "Power Screen";
		respawn_time = 20_sec;
	}
	else if (Q_strcasecmp(classname, "item_power_shield") == 0)
	{
		item_name = "Power Shield";
		respawn_time = 20_sec;
	}
	// Check for major powerups (300 second respawn = 5 minutes)
	else if (Q_strcasecmp(classname, "item_invulnerability") == 0)
	{
		item_name = "Invulnerability";
		respawn_time = 300_sec;
	}
	else if (Q_strcasecmp(classname, "item_invisibility") == 0)
	{
		item_name = "Invisibility";
		respawn_time = 300_sec;
	}
	// Check for lesser powerups (60 second respawn = 1 minute)
	else if (Q_strcasecmp(classname, "item_quad") == 0)
	{
		item_name = "Quad Damage";
		respawn_time = 60_sec;
	}
	else if (Q_strcasecmp(classname, "item_double") == 0)
	{
		item_name = "Double Damage";
		respawn_time = 60_sec;
	}
	else if (Q_strcasecmp(classname, "item_quadfire") == 0)
	{
		item_name = "DualFire Damage";
		respawn_time = 60_sec;
	}
	// Check for megahealth (20 second respawn after decay)
	else if (Q_strcasecmp(classname, "item_health_mega") == 0)
	{
		item_name = "Megahealth";
		respawn_time = 20_sec; // Respawn time after health decay finishes (health <= 100)
	}
	// Check for weapon items (30 second respawn - typical weapon respawn time)
	else if (ent->item->flags & IF_WEAPON)
	{
		// Get weapon respawn time from cvar (default 30 seconds)
		respawn_time = gtime_t::from_sec(g_weapon_respawn_time ? g_weapon_respawn_time->integer : 30);
		
		// Set friendly names for common weapons
		if (Q_strcasecmp(classname, "weapon_shotgun") == 0)
			item_name = "Shotgun";
		else if (Q_strcasecmp(classname, "weapon_supershotgun") == 0)
			item_name = "Super Shotgun";
		else if (Q_strcasecmp(classname, "weapon_machinegun") == 0)
			item_name = "Machinegun";
		else if (Q_strcasecmp(classname, "weapon_chaingun") == 0)
			item_name = "Chaingun";
		else if (Q_strcasecmp(classname, "weapon_grenadelauncher") == 0)
			item_name = "Grenade Launcher";
		else if (Q_strcasecmp(classname, "weapon_rocketlauncher") == 0)
			item_name = "Rocket Launcher";
		else if (Q_strcasecmp(classname, "weapon_hyperblaster") == 0)
			item_name = "Hyperblaster";
		else if (Q_strcasecmp(classname, "weapon_railgun") == 0)
			item_name = "Railgun";
		else if (Q_strcasecmp(classname, "weapon_bfg10k") == 0)
			item_name = "BFG10K";
		else
			item_name = "Weapon"; // Generic fallback
	}
	
	// Only continue if we identified this item type
	if (!item_name)
		return;

	// Create or update timing entry for this item
	map_trainer_t::timing_entry_t* timing_entry = MapTrainer_CreateOrUpdateTimingEntry(
		classname, item_name, ent->s.origin, level.time, respawn_time);
	
	if (timing_entry)
	{
		// Special handling for megahealth
		if (Q_strcasecmp(classname, "item_health_mega") == 0)
		{
			timing_entry->is_megahealth = true;
			timing_entry->megahealth_player = other;
			timing_entry->megahealth_decay_finished = false;
			timing_entry->megahealth_respawn_start = 0_ms;
			// Grace period lasts until health decay is finished
			timing_entry->grace_period_end = level.time + MEGAHEALTH_INDEFINITE_GRACE;
		}
		else
		{
			timing_entry->is_megahealth = false;
			timing_entry->megahealth_player = nullptr;
			timing_entry->megahealth_decay_finished = false;
			timing_entry->megahealth_respawn_start = 0_ms;
		}
		
		if (level.map_trainer.timing_debug_enabled)
		{
			gi.LocClient_Print(other, PRINT_HIGH, G_Fmt("[DEBUG] Pickup: {} at ({:.1f}, {:.1f}, {:.1f}) time {:.2f} respawn {:.2f}{}",
				item_name,
				ent->s.origin[0], ent->s.origin[1], ent->s.origin[2],
				level.time.seconds(),
				respawn_time.seconds(),
				timing_entry->is_megahealth ? " (MEGAHEALTH)" : ""
			).data());
		}
	}
	else
	{
		gi.LocClient_Print(other, PRINT_HIGH, "Warning: Too many concurrent timings active!");
	}

	// Show pickup message
	if (Q_strcasecmp(classname, "item_health_mega") == 0)
	{
		gi.LocClient_Print(other, PRINT_HIGH, "Megahealth - 20s timer after health < 100");
	}
	else
	{
		gi.LocClient_Print(other, PRINT_HIGH, G_Fmt("{} back in {:.0f} seconds", 
			item_name, respawn_time.seconds()).data());
	}
}

void MapTrainer_CheckArmorTiming(edict_t *player)
{
	if (!level.map_trainer.timing_enabled || !player->client)
		return;

	// Check all active timing entries
	for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
	{
		map_trainer_t::timing_entry_t* entry = &level.map_trainer.timing_entries[i];
		
		if (!entry->active)
			continue;
			
		// Skip megahealth entries (handled by separate function)
		if (entry->is_megahealth)
			continue;

		// Check if we're still in the grace period (5 seconds after pickup)
		if (level.time < entry->grace_period_end)
			continue;

		vec3_t diff = player->s.origin - entry->position;
		float distance = diff.length();

		if (distance <= ITEM_PICKUP_RADIUS) // Within pickup radius
		{
			gtime_t current_time = level.time;
			gtime_t expected_respawn_time = entry->pickup_time + entry->respawn_time;
			float time_diff = (current_time - expected_respawn_time).seconds();

		if (level.map_trainer.timing_debug_enabled)
		{
			// Print concise debug info when in radius and timing check is triggered
			gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("[DEBUG] {}: player({:.1f},{:.1f},{:.1f}) item({:.1f},{:.1f},{:.1f}) diff {:+.2f}",
				entry->item_name ? entry->item_name : "?",
				player->s.origin[0], player->s.origin[1], player->s.origin[2],
				entry->position[0], entry->position[1], entry->position[2],
				time_diff
			).data());
		}

			gi.LocClient_Print(player, PRINT_CENTER, G_Fmt("{}: {:+.2f}", entry->item_name ? entry->item_name : "?", time_diff).data());

			// Reset this timing entry after showing result
			entry->active = false;
			if (level.map_trainer.timing_entry_count > 0)
				level.map_trainer.timing_entry_count--;
		}
	}
}

void MapTrainer_CheckMegahealthTiming(edict_t *player)
{
	if (!level.map_trainer.timing_enabled || !player->client)
		return;

	// Check all active megahealth timing entries
	for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
	{
		map_trainer_t::timing_entry_t* entry = &level.map_trainer.timing_entries[i];
		
		if (!entry->active || !entry->is_megahealth)
			continue;
			
		// Verify this entry belongs to this player (with safety checks for disconnected players)
		if (!entry->megahealth_player || !entry->megahealth_player->inuse || 
		    !entry->megahealth_player->client)
		{
			// Player disconnected - invalidate this entry
			entry->active = false;
			if (level.map_trainer.timing_entry_count > 0)
				level.map_trainer.timing_entry_count--;
			continue;
		}
		
		if (entry->megahealth_player != player)
			continue;

		// Phase 1: Check if health decay has finished
		if (!entry->megahealth_decay_finished)
		{
			// If player is dead, invalidate this entry (death is not normal decay)
			if (player->health <= 0 || player->deadflag)
			{
				entry->active = false;
				if (level.map_trainer.timing_entry_count > 0)
					level.map_trainer.timing_entry_count--;
				
				if (level.map_trainer.timing_debug_enabled)
				{
					gi.LocClient_Print(player, PRINT_HIGH, "[DEBUG] Megahealth timing cancelled (player died)");
				}
				continue;
			}
			
			// Check if player's health has dropped to max_health or below
			if (player->health <= player->max_health)
			{
				// Health decay finished, start the 20-second respawn timer
				entry->megahealth_decay_finished = true;
				entry->megahealth_respawn_start = level.time;
				// Add a small grace period to prevent immediate timing results if player is near spawn
				entry->grace_period_end = level.time + MEGAHEALTH_PHASE2_GRACE;
				
				if (level.map_trainer.timing_debug_enabled)
				{
					gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("[DEBUG] Megahealth decay finished at {:.2f}, starting 20s respawn timer (+{:.0f}s grace)",
						level.time.seconds(), MEGAHEALTH_PHASE2_GRACE.seconds()).data());
				}
			}
			// Still in decay phase, continue monitoring
			continue;
		}

		// Phase 2: Check if player is near the megahealth spawn point for timing results
		
		// Check if still in grace period after decay
		if (level.time < entry->grace_period_end)
			continue;
		
		// Check for timeout - auto-expire if player hasn't returned within timeout period
		gtime_t time_since_decay = level.time - entry->megahealth_respawn_start;
		if (time_since_decay > MEGAHEALTH_EXPIRE_TIMEOUT)
		{
			entry->active = false;
			if (level.map_trainer.timing_entry_count > 0)
				level.map_trainer.timing_entry_count--;
			
			if (level.map_trainer.timing_debug_enabled)
			{
				gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("[DEBUG] Megahealth timing expired after {:.0f}s (no return to spawn)",
					MEGAHEALTH_EXPIRE_TIMEOUT.seconds()).data());
			}
			continue;
		}
		
		vec3_t diff = player->s.origin - entry->position;
		float distance = diff.length();

		if (distance <= ITEM_PICKUP_RADIUS) // Within pickup radius
		{
			gtime_t current_time = level.time;
			gtime_t expected_respawn_time = entry->megahealth_respawn_start + entry->respawn_time;
			float time_diff = (current_time - expected_respawn_time).seconds();

			if (level.map_trainer.timing_debug_enabled)
			{
				gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("[DEBUG] Megahealth timing check: decay_start={:.2f} respawn_start={:.2f} expected={:.2f} now={:.2f} diff={:+.2f}",
					entry->pickup_time.seconds(),
					entry->megahealth_respawn_start.seconds(), 
					expected_respawn_time.seconds(),
					current_time.seconds(),
					time_diff
				).data());
			}

			gi.LocClient_Print(player, PRINT_CENTER, G_Fmt("Megahealth: {:+.2f}", time_diff).data());

			// Reset this timing entry after showing result
			entry->active = false;
			if (level.map_trainer.timing_entry_count > 0)
				level.map_trainer.timing_entry_count--;
		}
	}
}
