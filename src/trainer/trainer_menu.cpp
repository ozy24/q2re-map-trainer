// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"

// ==================== VERSION ====================

#define TRAINER_VERSION "v0.93 beta by ozy"

// ==================== MENU SYSTEM ====================

void MapTrainer_MenuClose(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Close(ent);
}

void MapTrainer_RestartPathTraining()
{
	if (level.map_trainer.training_enabled)
	{
		// Rebuild the item list with new category settings
		MapTrainer_BuildItemList(level.mapname);
		
		// Reset the training state so player can pick up any item to begin
		level.map_trainer.first_pickup = true;
		level.map_trainer.current_target_index = -1;
		level.map_trainer.previous_target_index = -1;
	}
}

// ==================== PATH TRAINING MENU ====================

void MapTrainer_ToggleWeapons(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.weapons_enabled = !level.map_trainer.weapons_enabled;
	MapTrainer_RestartPathTraining();
	PMenu_Update(ent);
}

void MapTrainer_ToggleAmmo(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.ammo_enabled = !level.map_trainer.ammo_enabled;
	MapTrainer_RestartPathTraining();
	PMenu_Update(ent);
}

void MapTrainer_ToggleHealth(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.health_enabled = !level.map_trainer.health_enabled;
	MapTrainer_RestartPathTraining();
	PMenu_Update(ent);
}

void MapTrainer_ToggleArmor(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.armor_enabled = !level.map_trainer.armor_enabled;
	MapTrainer_RestartPathTraining();
	PMenu_Update(ent);
}

void MapTrainer_TogglePowerups(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.powerups_enabled = !level.map_trainer.powerups_enabled;
	MapTrainer_RestartPathTraining();
	PMenu_Update(ent);
}

void MapTrainer_ToggleSpeedometer(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.speedometer_enabled = !level.map_trainer.speedometer_enabled;
	PMenu_Update(ent);
}

void MapTrainer_ToggleTraining(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.training_enabled = !level.map_trainer.training_enabled;
	
	// If training mode was just turned ON, load CSV and reset the training state
	if (level.map_trainer.training_enabled)
	{
		// Mutual exclusion: disable timing trainer if it's enabled
		if (level.map_trainer.timing_enabled)
		{
			level.map_trainer.timing_enabled = false;
			// Reset all active timings when disabled
			for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
			{
				level.map_trainer.timing_entries[i].active = false;
			}
			level.map_trainer.timing_entry_count = 0;
			gi.LocClient_Print(ent, PRINT_HIGH, "Item Timing Trainer automatically disabled.");
		}
		
		MapTrainer_BuildItemList(level.mapname);
		level.map_trainer.first_pickup = true;
		level.map_trainer.current_target_index = -1;
		level.map_trainer.previous_target_index = -1;
		
		// Give immediate feedback about item loading
		if (level.map_trainer.initialized)
		{
			gi.LocClient_Print(ent, PRINT_HIGH, "Item Path Trainer enabled. Items loaded from map.");
		}
		else
		{
			gi.LocClient_Print(ent, PRINT_HIGH, "No items found in map. Item Path Trainer disabled.");
			level.map_trainer.training_enabled = false; // Auto-disable if no items
		}
	}
	else
	{
		// Training mode turned OFF - clean up item data
		if (level.map_trainer.items)
		{
			gi.TagFree(level.map_trainer.items);
			level.map_trainer.items = nullptr;
		}
		level.map_trainer.item_count = 0;
		level.map_trainer.initialized = false;
		level.map_trainer.current_target_index = -1;
		level.map_trainer.previous_target_index = -1;
		
		gi.LocClient_Print(ent, PRINT_HIGH, "Item Path Trainer disabled.");
	}
	
	PMenu_Update(ent);
}

void MapTrainer_ToggleCombineHealthPacks(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.combine_health_packs = !level.map_trainer.combine_health_packs;
	MapTrainer_RestartPathTraining();
	PMenu_Update(ent);
}

void MapTrainer_UpdateItemPathingSubmenu(edict_t *ent)
{
	if (!ent->client->menu)
		return;
		
	pmenu_t *entries = ent->client->menu->entries;
	
	// Update toggle display text
	Q_strlcpy(entries[2].text, G_Fmt("Path Trainer: {}", level.map_trainer.training_enabled ? "Enabled" : "Disabled").data(), sizeof(entries[2].text));
	
	if (level.map_trainer.training_enabled)
	{
		// Show item category options when training is enabled
		Q_strlcpy(entries[3].text, G_Fmt("Weapons: {}", level.map_trainer.weapons_enabled ? "ON" : "OFF").data(), sizeof(entries[3].text));
		Q_strlcpy(entries[4].text, G_Fmt("Ammo: {}", level.map_trainer.ammo_enabled ? "ON" : "OFF").data(), sizeof(entries[4].text));
		Q_strlcpy(entries[5].text, G_Fmt("Health: {}", level.map_trainer.health_enabled ? "ON" : "OFF").data(), sizeof(entries[5].text));
		Q_strlcpy(entries[6].text, G_Fmt("Armor: {}", level.map_trainer.armor_enabled ? "ON" : "OFF").data(), sizeof(entries[6].text));
		Q_strlcpy(entries[7].text, G_Fmt("Powerups: {}", level.map_trainer.powerups_enabled ? "ON" : "OFF").data(), sizeof(entries[7].text));
		Q_strlcpy(entries[8].text, G_Fmt("Combine Health Packs: {}", level.map_trainer.combine_health_packs ? "ON" : "OFF").data(), sizeof(entries[8].text));
		
		// Re-enable the function pointers
		entries[3].SelectFunc = MapTrainer_ToggleWeapons;
		entries[4].SelectFunc = MapTrainer_ToggleAmmo;
		entries[5].SelectFunc = MapTrainer_ToggleHealth;
		entries[6].SelectFunc = MapTrainer_ToggleArmor;
		entries[7].SelectFunc = MapTrainer_TogglePowerups;
		entries[8].SelectFunc = MapTrainer_ToggleCombineHealthPacks;
	}
	else
	{
		// Hide item category options when training is disabled
		Q_strlcpy(entries[3].text, "", sizeof(entries[3].text));
		Q_strlcpy(entries[4].text, "", sizeof(entries[4].text));
		Q_strlcpy(entries[5].text, "", sizeof(entries[5].text));
		Q_strlcpy(entries[6].text, "", sizeof(entries[6].text));
		Q_strlcpy(entries[7].text, "", sizeof(entries[7].text));
		Q_strlcpy(entries[8].text, "", sizeof(entries[8].text));
		Q_strlcpy(entries[9].text, "", sizeof(entries[9].text)); // Hide the blank line too
		
		// Disable the function pointers
		entries[3].SelectFunc = nullptr;
		entries[4].SelectFunc = nullptr;
		entries[5].SelectFunc = nullptr;
		entries[6].SelectFunc = nullptr;
		entries[7].SelectFunc = nullptr;
		entries[8].SelectFunc = nullptr;
	}
}

pmenu_t maptrainer_itempathing_submenu[] = {
	{ "Item Pathing Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Training Mode: OFF", PMENU_ALIGN_LEFT, MapTrainer_ToggleTraining },
	{ "Weapons: ON", PMENU_ALIGN_LEFT, MapTrainer_ToggleWeapons },
	{ "Ammo: ON", PMENU_ALIGN_LEFT, MapTrainer_ToggleAmmo },
	{ "Health: ON", PMENU_ALIGN_LEFT, MapTrainer_ToggleHealth },
	{ "Armor: ON", PMENU_ALIGN_LEFT, MapTrainer_ToggleArmor },
	{ "Powerups: ON", PMENU_ALIGN_LEFT, MapTrainer_TogglePowerups },
			{ "Combine Health Packs: OFF", PMENU_ALIGN_LEFT, MapTrainer_ToggleCombineHealthPacks },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Back to Main Menu", PMENU_ALIGN_LEFT, MapTrainer_BackToMainMenu },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Q2RE Map Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ TRAINER_VERSION, PMENU_ALIGN_CENTER, nullptr }
};

void MapTrainer_OpenItemPathingSubmenu(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Open(ent, maptrainer_itempathing_submenu, -1, sizeof(maptrainer_itempathing_submenu) / sizeof(pmenu_t), nullptr, MapTrainer_UpdateItemPathingSubmenu);
}

// ==================== TIMING TRAINER MENU ====================

void MapTrainer_ToggleTiming(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.timing_enabled = !level.map_trainer.timing_enabled;
	
	if (level.map_trainer.timing_enabled)
	{
		// Mutual exclusion: disable path trainer if it's enabled
		if (level.map_trainer.training_enabled)
		{
			level.map_trainer.training_enabled = false;
			// Clean up item data
			if (level.map_trainer.items)
			{
				gi.TagFree(level.map_trainer.items);
				level.map_trainer.items = nullptr;
			}
			level.map_trainer.item_count = 0;
			level.map_trainer.initialized = false;
			level.map_trainer.current_target_index = -1;
			level.map_trainer.previous_target_index = -1;
			gi.LocClient_Print(ent, PRINT_HIGH, "Item Path Trainer automatically disabled.");
		}
		
		gi.LocClient_Print(ent, PRINT_HIGH, "Item Timing Trainer enabled.");
	}
	else
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Item Timing Trainer disabled.");
		// Reset all active timings when disabled
		for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
		{
			level.map_trainer.timing_entries[i].active = false;
		}
		level.map_trainer.timing_entry_count = 0;
	}
	
	PMenu_Update(ent);
}

void MapTrainer_ToggleFreeCollect(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.free_collect_enabled = !level.map_trainer.free_collect_enabled;
	
	if (level.map_trainer.free_collect_enabled)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Free Collect enabled.");
	}
	else
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Free Collect disabled.");
	}
	
	PMenu_Update(ent);
}

void MapTrainer_ToggleTimingDebug(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.timing_debug_enabled = !level.map_trainer.timing_debug_enabled;
	
	if (level.map_trainer.timing_debug_enabled)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Timing Debug enabled.");
	}
	else
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Timing Debug disabled.");
	}
	
	PMenu_Update(ent);
}

void MapTrainer_ToggleMajorItemsOnly(edict_t *ent, pmenuhnd_t *p)
{
	level.map_trainer.timing_major_items_only = !level.map_trainer.timing_major_items_only;
	
	if (level.map_trainer.timing_major_items_only)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Tracking: Major items only (RL/RG/CG + Armors + MH + Quad)");
	}
	else
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Tracking: All items");
	}
	
	PMenu_Update(ent);
}

void MapTrainer_UpdateItemTimingSubmenu(edict_t *ent)
{
	if (!ent->client->menu)
		return;
		
	pmenu_t *entries = ent->client->menu->entries;
	
	// Update toggle display text
	Q_strlcpy(entries[2].text, G_Fmt("Timing Trainer: {}", level.map_trainer.timing_enabled ? "Enabled" : "Disabled").data(), sizeof(entries[2].text));
	Q_strlcpy(entries[3].text, G_Fmt("Track: {}", level.map_trainer.timing_major_items_only ? "Major Items" : "All Items").data(), sizeof(entries[3].text));
	Q_strlcpy(entries[4].text, G_Fmt("Free Collect: {}", level.map_trainer.free_collect_enabled ? "ON" : "OFF").data(), sizeof(entries[4].text));
	Q_strlcpy(entries[5].text, G_Fmt("Debug Prints: {}", level.map_trainer.timing_debug_enabled ? "ON" : "OFF").data(), sizeof(entries[5].text));
}

pmenu_t maptrainer_itemtiming_submenu[] = {
	{ "Item Timing Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Timing Trainer: Disabled", PMENU_ALIGN_LEFT, MapTrainer_ToggleTiming },
	{ "Track: Major Items", PMENU_ALIGN_LEFT, MapTrainer_ToggleMajorItemsOnly },
	{ "Free Collect: ON", PMENU_ALIGN_LEFT, MapTrainer_ToggleFreeCollect },
	{ "Debug Prints: OFF", PMENU_ALIGN_LEFT, MapTrainer_ToggleTimingDebug },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Back to Main Menu", PMENU_ALIGN_LEFT, MapTrainer_BackToMainMenu },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Q2RE Map Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ TRAINER_VERSION, PMENU_ALIGN_CENTER, nullptr }
};

void MapTrainer_OpenItemTimingSubmenu(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Open(ent, maptrainer_itemtiming_submenu, -1, sizeof(maptrainer_itemtiming_submenu) / sizeof(pmenu_t), nullptr, MapTrainer_UpdateItemTimingSubmenu);
}

// ==================== JUMP TRAINER MENU ====================

void MapTrainer_SavePosition(edict_t *ent, pmenuhnd_t *p)
{
	// Execute savepos command and close menu
	gi.AddCommandString("savepos\n");
	PMenu_Close(ent);
}

void MapTrainer_LoadPosition(edict_t *ent, pmenuhnd_t *p)
{
	// Execute loadpos command and close menu
	gi.AddCommandString("loadpos\n");
	PMenu_Close(ent);
}

void MapTrainer_ToggleBhopTrainer(edict_t *ent, pmenuhnd_t *p)
{
    level.map_trainer.bhop_enabled = !level.map_trainer.bhop_enabled;
    gi.LocClient_Print(ent, PRINT_HIGH, "Bhop trainer {}", level.map_trainer.bhop_enabled ? "enabled" : "disabled");
    PMenu_Update(ent);
}

void MapTrainer_UpdateJumpTrainerSubmenu(edict_t *ent)
{
    if (!ent->client || !ent->client->menu)
        return;

    pmenu_t *entries = ent->client->menu->entries;
    // entry index 4 is bhop toggle in this submenu
    Q_strlcpy(entries[4].text, G_Fmt("Bhop Consistency: {}", level.map_trainer.bhop_enabled ? "ON" : "OFF").data(), sizeof(entries[4].text));
}

pmenu_t maptrainer_jumptrainer_submenu[] = {
	{ "Jump Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Save Position (savepos)", PMENU_ALIGN_LEFT, MapTrainer_SavePosition },
	{ "Load Position (loadpos)", PMENU_ALIGN_LEFT, MapTrainer_LoadPosition },
    { "Bhop Consistency: OFF", PMENU_ALIGN_LEFT, MapTrainer_ToggleBhopTrainer },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Back to Main Menu", PMENU_ALIGN_LEFT, MapTrainer_BackToMainMenu },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Q2RE Map Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ TRAINER_VERSION, PMENU_ALIGN_CENTER, nullptr }
};

void MapTrainer_OpenJumpTrainerSubmenu(edict_t *ent, pmenuhnd_t *p)
{
    PMenu_Open(ent, maptrainer_jumptrainer_submenu, -1, sizeof(maptrainer_jumptrainer_submenu) / sizeof(pmenu_t), nullptr, MapTrainer_UpdateJumpTrainerSubmenu);
}

// ==================== MAIN MENU ====================

// Forward declaration
void MapTrainer_OpenMenu(edict_t *ent);

void MapTrainer_BackToMainMenu(edict_t *ent, pmenuhnd_t *p)
{
	MapTrainer_OpenMenu(ent);
}

void MapTrainer_UpdateMenu(edict_t *ent)
{
	if (!ent->client->menu)
		return;
		
	pmenu_t *entries = ent->client->menu->entries;
	
	// Update speedometer display text (index 6 in the main menu - after adding Item Jump Trainer)
	Q_strlcpy(entries[6].text, G_Fmt("Speedometer: {}", level.map_trainer.speedometer_enabled ? "ON" : "OFF").data(), sizeof(entries[6].text));
}

pmenu_t maptrainer_menu[] = {
	{ "Settings", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Item Path Trainer", PMENU_ALIGN_LEFT, MapTrainer_OpenItemPathingSubmenu },
	{ "Item Timing Trainer", PMENU_ALIGN_LEFT, MapTrainer_OpenItemTimingSubmenu },
	{ "Item Jump Trainer", PMENU_ALIGN_LEFT, MapTrainer_OpenJumpTrainerSubmenu },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Speedometer: ON", PMENU_ALIGN_LEFT, MapTrainer_ToggleSpeedometer },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Close Menu", PMENU_ALIGN_LEFT, MapTrainer_MenuClose },
	{ "", PMENU_ALIGN_CENTER, nullptr },
	{ "Q2RE Map Trainer", PMENU_ALIGN_CENTER, nullptr },
	{ TRAINER_VERSION, PMENU_ALIGN_CENTER, nullptr }
	
};

void MapTrainer_OpenMenu(edict_t *ent)
{
	PMenu_Open(ent, maptrainer_menu, -1, sizeof(maptrainer_menu) / sizeof(pmenu_t), nullptr, MapTrainer_UpdateMenu);
}

void Cmd_MapTrainerMenu_f(edict_t *ent)
{
	if (!ent->client)
		return;
		
	MapTrainer_OpenMenu(ent);
}

