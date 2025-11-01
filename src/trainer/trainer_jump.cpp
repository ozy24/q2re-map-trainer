// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"

// ==================== JUMP TRAINER FEATURE ====================

void MapTrainer_UpdateSpeedometer(edict_t *player)
{
	if (!player || !player->client) return;
	
	if (!level.map_trainer.speedometer_enabled) {
		// Clear speedometer when disabled
		player->client->ps.stats[STAT_SPEEDOMETER] = 0;
		return;
	}
	
	// Calculate horizontal speed (ignoring Z component for movement-focused display)
	gvec3_t horizontal_velocity = player->velocity;
	horizontal_velocity[2] = 0; // Remove vertical component
	
	float speed = horizontal_velocity.length();
	
	// Set the speed as a player stat (clamped to 16-bit signed integer range)
	player->client->ps.stats[STAT_SPEEDOMETER] = static_cast<int16_t>(std::min(speed, 32767.0f));
}

// ==================== PRACTICE SPAWN COMMANDS ====================

void Cmd_SetSpawn_f(edict_t *ent)
{
	if (!ent->client)
		return;
		
	// Save current position and angles
	level.map_trainer.practice_spawn_origin = ent->s.origin;
	level.map_trainer.practice_spawn_angles = ent->client->ps.viewangles;
	level.map_trainer.practice_spawn_set = true;
	
	gi.LocClient_Print(ent, PRINT_HIGH, "Position saved");
}

void Cmd_WarpSpawn_f(edict_t *ent)
{
	if (!ent->client)
		return;
		
	if (!level.map_trainer.practice_spawn_set)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "No saved position. Use 'savepos' first.");
		return;
	}
	
	// Teleport player to saved position
	ent->s.origin = level.map_trainer.practice_spawn_origin;
	ent->client->ps.pmove.origin = level.map_trainer.practice_spawn_origin;
	
	// Set saved view angles - need to set multiple angle fields to ensure it takes effect
	ent->client->ps.viewangles = level.map_trainer.practice_spawn_angles;
	ent->client->v_angle = level.map_trainer.practice_spawn_angles;
	ent->s.angles = level.map_trainer.practice_spawn_angles;
	
	// Force the client to use these angles by setting the delta angles
	// This prevents the client from overriding with their current mouse position
	ent->client->ps.pmove.delta_angles = level.map_trainer.practice_spawn_angles - ent->client->resp.cmd_angles;
	
	// Clear velocity to prevent momentum from carrying over
	ent->velocity = vec3_origin;
	ent->client->ps.pmove.velocity = vec3_origin;
	
	// Link entity at new position
	gi.linkentity(ent);
	
	gi.LocClient_Print(ent, PRINT_HIGH, "Position loaded");
}

// ==================== BHOP CONSISTENCY TRACKING ====================

void MapTrainer_UpdateBhopTracking(edict_t *player, pmove_t &pm)
{
	if (!player || !player->client)
		return;
	
	// Detect landing transition this frame
	bool was_airborne = !(player->client->ps.pmove.pm_flags & PMF_ON_GROUND);
	bool now_grounded = (pm.s.pm_flags & PMF_ON_GROUND) != 0;
	
	if (was_airborne && now_grounded)
	{
		// Player just landed - only track if chain is active or about to start
		TrainerLog("BHOP", "LANDED: chain_active=%d, recently_landed=%d->1, frames=%d->0",
			player->client->bhop_chain_active,
			player->client->bhop_recently_landed,
			player->client->bhop_grounded_frames_since_landing);
		
		player->client->bhop_recently_landed = true;
		player->client->bhop_grounded_frames_since_landing = 0;
		player->client->bhop_jump_held_on_landing = !!(pm.cmd.buttons & BUTTON_JUMP) || !!(player->client->ps.pmove.pm_flags & PMF_JUMP_HELD);
	}

	// Only process timing if chain is active
	if (player->client->bhop_chain_active)
	{
		// Count grounded frames after landing
		if (player->client->bhop_recently_landed && now_grounded)
		{
			player->client->bhop_grounded_frames_since_landing++;
			if (player->client->bhop_grounded_frames_since_landing % 10 == 0) // Log every 10 frames
			{
			TrainerLog("BHOP", "FRAME_COUNT: chain_active=1, frames=%d",
				player->client->bhop_grounded_frames_since_landing);
			}
		}

		// End bhop chain if grounded too long (30 frames ≈ 1 second at 30fps, 0.5s at 60fps)
		if (now_grounded && player->client->bhop_grounded_frames_since_landing > 30)
		{
		TrainerLog("BHOP", "TIMEOUT: chain_active=1->0, frames=%d, resetting state",
			player->client->bhop_grounded_frames_since_landing);
			
			player->client->bhop_chain_active = false;
			// Reset landing state to prevent stale data from affecting next chain
			player->client->bhop_recently_landed = false;
			player->client->bhop_grounded_frames_since_landing = 0;
		}
	}

	// Classify when a jump actually happens
	if (pm.jump_sound && !(pm.s.pm_flags & PMF_ON_LADDER))
	{
		bool perfect = player->client->bhop_recently_landed && player->client->bhop_grounded_frames_since_landing <= 1;
		bool late = player->client->bhop_recently_landed && player->client->bhop_grounded_frames_since_landing > 1;
		bool early_or_held = player->client->bhop_recently_landed && player->client->bhop_jump_held_on_landing && player->client->bhop_grounded_frames_since_landing <= 1;

	TrainerLog("BHOP", "JUMP: chain_active=%d, recently_landed=%d, frames=%d, perfect=%d, late=%d",
		player->client->bhop_chain_active,
		player->client->bhop_recently_landed,
		player->client->bhop_grounded_frames_since_landing,
		perfect, late);

		// Only track and show feedback if we're in an active bhop chain (not the first jump)
		if (player->client->bhop_chain_active)
		{
			// Update rolling window (20 recent jumps)
			const uint8_t window_size = (uint8_t) q_countof(player->client->bhop_result_window);
			if (player->client->bhop_result_window_count < window_size)
				player->client->bhop_result_window_count++;
			player->client->bhop_result_window[player->client->bhop_result_window_index] = perfect;
			player->client->bhop_result_window_index = (player->client->bhop_result_window_index + 1) % window_size;

			// Simple feedback
			if (perfect)
			{
				TrainerLog("BHOP", "  -> Feedback: Perfect");
				gi.LocClient_Print(player, PRINT_HIGH, "Bhop: Perfect");
				// Audible feedback for frame-perfect
				gi.sound(player, CHAN_AUTO, gi.soundindex("misc/menu3.wav"), 1.0f, ATTN_NONE, 0);
			}
			else if (early_or_held)
			{
				TrainerLog("BHOP", "  -> Feedback: Early/Held");
				gi.LocClient_Print(player, PRINT_HIGH, "Bhop: Early/Held");
			}
			else if (late)
			{
				TrainerLog("BHOP", "  -> Feedback: Late (%df)", player->client->bhop_grounded_frames_since_landing);
				gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("Bhop: Late ({}f)", player->client->bhop_grounded_frames_since_landing).data());
			}
		}
		else
		{
		// First jump - start the chain but don't count it
		TrainerLog("BHOP", "  -> Starting chain (no feedback)");
		player->client->bhop_chain_active = true;
	}

	// Reset landing state after jump
	TrainerLog("BHOP", "  -> Reset: recently_landed=0, frames=0");
		player->client->bhop_recently_landed = false;
		player->client->bhop_grounded_frames_since_landing = 0;
		player->client->bhop_jump_held_on_landing = false;
	}
}

