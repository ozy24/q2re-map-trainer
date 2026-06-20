// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include "trainer_config.h"

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
	player->client->ps.stats[STAT_SPEEDOMETER] = static_cast<int16_t>(std::min(speed, trainer_config::SPEEDOMETER_MAX_DISPLAY));
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

namespace
{
constexpr gtime_t BHOP_CHAIN_TIMEOUT = gtime_t::from_sec(trainer_config::BHOP_CHAIN_TIMEOUT_SEC);

gtime_t MapTrainer_BhopPerfectMax()
{
	return gtime_t::from_ms(std::max<int64_t>(gi.frame_time_ms, 1));
}

gtime_t MapTrainer_BhopGroundedDuration(const gclient_t *client)
{
	if (!client || !client->bhop_recently_landed)
		return gtime_t();
	return level.time - client->bhop_landing_time;
}
} // namespace

void MapTrainer_UpdateBhopTracking(edict_t *player, pmove_t &pm)
{
	if (!player || !player->client)
		return;
	
	// Detect landing transition this frame
	bool was_airborne = !(player->client->ps.pmove.pm_flags & PMF_ON_GROUND);
	bool now_grounded = (pm.s.pm_flags & PMF_ON_GROUND) != 0;
	
	if (was_airborne && now_grounded)
	{
		TrainerLog("BHOP", "LANDED: chain_active=%d, recently_landed=%d->1",
			player->client->bhop_chain_active,
			player->client->bhop_recently_landed);
		
		player->client->bhop_recently_landed = true;
		player->client->bhop_landing_time = level.time;
		player->client->bhop_jump_held_on_landing = !!(pm.cmd.buttons & BUTTON_JUMP) || !!(player->client->ps.pmove.pm_flags & PMF_JUMP_HELD);
	}

	if (player->client->bhop_chain_active)
	{
		const gtime_t grounded_duration = MapTrainer_BhopGroundedDuration(player->client);

		if (now_grounded && player->client->bhop_recently_landed && grounded_duration > BHOP_CHAIN_TIMEOUT)
		{
			TrainerLog("BHOP", "TIMEOUT: chain_active=1->0, grounded_ms=%lld, resetting state",
				static_cast<long long>(grounded_duration.milliseconds()));
			
			player->client->bhop_chain_active = false;
			player->client->bhop_recently_landed = false;
			player->client->bhop_landing_time = gtime_t();
		}
	}

	if (pm.jump_sound && !(pm.s.pm_flags & PMF_ON_LADDER))
	{
		const gtime_t grounded_duration = MapTrainer_BhopGroundedDuration(player->client);
		const gtime_t perfect_max = MapTrainer_BhopPerfectMax();
		const bool perfect = player->client->bhop_recently_landed && grounded_duration <= perfect_max;
		const bool late = player->client->bhop_recently_landed && grounded_duration > perfect_max;
		const bool early_or_held = player->client->bhop_recently_landed && player->client->bhop_jump_held_on_landing && grounded_duration <= perfect_max;

		TrainerLog("BHOP", "JUMP: chain_active=%d, recently_landed=%d, grounded_ms=%lld, perfect=%d, late=%d",
			player->client->bhop_chain_active,
			player->client->bhop_recently_landed,
			static_cast<long long>(grounded_duration.milliseconds()),
			perfect, late);

		if (player->client->bhop_chain_active)
		{
			const uint8_t window_size = (uint8_t) q_countof(player->client->bhop_result_window);
			if (player->client->bhop_result_window_count < window_size)
				player->client->bhop_result_window_count++;
			player->client->bhop_result_window[player->client->bhop_result_window_index] = perfect;
			player->client->bhop_result_window_index = (player->client->bhop_result_window_index + 1) % window_size;

			if (perfect)
			{
				TrainerLog("BHOP", "  -> Feedback: Perfect");
				gi.LocClient_Print(player, PRINT_HIGH, "Bhop: Perfect");
				gi.sound(player, CHAN_AUTO, gi.soundindex("misc/menu3.wav"), trainer_config::SOUND_VOLUME_FULL, ATTN_NONE, 0);
			}
			else if (early_or_held)
			{
				TrainerLog("BHOP", "  -> Feedback: Early/Held");
				gi.LocClient_Print(player, PRINT_HIGH, "Bhop: Early/Held");
			}
			else if (late)
			{
				const int64_t late_ms = grounded_duration.milliseconds();
				TrainerLog("BHOP", "  -> Feedback: Late (%lldms)", static_cast<long long>(late_ms));
				gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("Bhop: Late ({}ms)", late_ms).data());
			}
		}
		else
		{
			TrainerLog("BHOP", "  -> Starting chain (no feedback)");
			player->client->bhop_chain_active = true;
		}

		TrainerLog("BHOP", "  -> Reset landing state");
		player->client->bhop_recently_landed = false;
		player->client->bhop_landing_time = gtime_t();
		player->client->bhop_jump_held_on_landing = false;
	}
}
