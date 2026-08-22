// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include "trainer_cvars.h"
#include "trainer_config.h"

namespace
{
cvar_t *trainer_weapons;
cvar_t *trainer_ammo;
cvar_t *trainer_health;
cvar_t *trainer_armor;
cvar_t *trainer_powerups;
cvar_t *trainer_speedometer;
cvar_t *trainer_mode;
cvar_t *trainer_combine_health;
cvar_t *trainer_path_stimpacks;
cvar_t *trainer_path_armor_shards;
cvar_t *trainer_free_collect;
cvar_t *trainer_timing_major_only;
cvar_t *trainer_timing_challenge;
cvar_t *trainer_bhop;
cvar_t *trainer_spawn_random;
cvar_t *trainer_spawn_beacon;
cvar_t *trainer_spawn_intent;
cvar_t *trainer_ghost;
cvar_t *trainer_ghost_timings;
cvar_t *trainer_ghost_skill;
cvar_t *trainer_ghost_body;
cvar_t *trainer_ghost_adaptive;
cvar_t *trainer_hud_level;
cvar_t *trainer_match_length;
cvar_t *trainer_silent_feedback;

bool CvarToBool(cvar_t *cv)
{
	return cv && cv->integer != 0;
}

void CvarSetBool(const char *name, bool value)
{
	gi.cvar_set(name, value ? "1" : "0");
}
} // namespace

void MapTrainer_RegisterCvars()
{
	trainer_weapons = gi.cvar("trainer_weapons", "1", CVAR_ARCHIVE);
	trainer_ammo = gi.cvar("trainer_ammo", "1", CVAR_ARCHIVE);
	trainer_health = gi.cvar("trainer_health", "1", CVAR_ARCHIVE);
	trainer_armor = gi.cvar("trainer_armor", "1", CVAR_ARCHIVE);
	trainer_powerups = gi.cvar("trainer_powerups", "1", CVAR_ARCHIVE);
	trainer_speedometer = gi.cvar("trainer_speedometer", "1", CVAR_ARCHIVE);
	trainer_mode = gi.cvar("trainer_mode", "0", CVAR_ARCHIVE);
	trainer_combine_health = gi.cvar("trainer_combine_health", "0", CVAR_ARCHIVE);
	trainer_path_stimpacks = gi.cvar("trainer_path_stimpacks", "1", CVAR_ARCHIVE);
	trainer_path_armor_shards = gi.cvar("trainer_path_armor_shards", "1", CVAR_ARCHIVE);
	trainer_free_collect = gi.cvar("trainer_free_collect", "1", CVAR_ARCHIVE);
	trainer_timing_major_only = gi.cvar("trainer_timing_major_only", "1", CVAR_ARCHIVE);
	trainer_timing_challenge = gi.cvar("trainer_timing_challenge", "0", CVAR_ARCHIVE);
	trainer_bhop = gi.cvar("trainer_bhop", "0", CVAR_ARCHIVE);
	trainer_spawn_random = gi.cvar("trainer_spawn_random", "0", CVAR_ARCHIVE);
	trainer_spawn_beacon = gi.cvar("trainer_spawn_beacon", "1", CVAR_ARCHIVE);
	trainer_spawn_intent = gi.cvar("trainer_spawn_intent", "0", CVAR_ARCHIVE);
	trainer_ghost = gi.cvar("trainer_ghost", "0", CVAR_ARCHIVE);
	trainer_ghost_timings = gi.cvar("trainer_ghost_timings", "1", CVAR_ARCHIVE);
	trainer_ghost_skill = gi.cvar("trainer_ghost_skill", "3", CVAR_ARCHIVE);
	trainer_ghost_body = gi.cvar("trainer_ghost_body", "2", CVAR_ARCHIVE);
	trainer_ghost_adaptive = gi.cvar("trainer_ghost_adaptive", "1", CVAR_ARCHIVE);
	trainer_hud_level = gi.cvar("trainer_hud_level", "3", CVAR_ARCHIVE);
	trainer_match_length = gi.cvar("trainer_match_length", "600", CVAR_ARCHIVE);
	trainer_silent_feedback = gi.cvar("trainer_silent_feedback", "0", CVAR_ARCHIVE);
}

void MapTrainer_LoadFromCvars()
{
	level.map_trainer.weapons_enabled = CvarToBool(trainer_weapons);
	level.map_trainer.ammo_enabled = CvarToBool(trainer_ammo);
	level.map_trainer.health_enabled = CvarToBool(trainer_health);
	level.map_trainer.armor_enabled = CvarToBool(trainer_armor);
	level.map_trainer.powerups_enabled = CvarToBool(trainer_powerups);
	level.map_trainer.speedometer_enabled = CvarToBool(trainer_speedometer);
	level.map_trainer.trainer_mode = static_cast<trainer_mode_t>(trainer_mode ? trainer_mode->integer : 0);
	level.map_trainer.combine_health_packs = CvarToBool(trainer_combine_health);
	level.map_trainer.path_stimpacks_enabled = CvarToBool(trainer_path_stimpacks);
	level.map_trainer.path_armor_shards_enabled = CvarToBool(trainer_path_armor_shards);
	level.map_trainer.free_collect_enabled = CvarToBool(trainer_free_collect);
	level.map_trainer.timing_major_items_only = CvarToBool(trainer_timing_major_only);
	level.map_trainer.timing_challenge_mode = static_cast<timing_challenge_mode_t>(
		trainer_timing_challenge ? trainer_timing_challenge->integer : 0);
	level.map_trainer.bhop_enabled = CvarToBool(trainer_bhop);
	level.map_trainer.spawn_trainer_true_random = CvarToBool(trainer_spawn_random);
	level.map_trainer.spawn_trainer_beacon_enabled = CvarToBool(trainer_spawn_beacon);
	level.map_trainer.spawn_trainer_intent = CvarToBool(trainer_spawn_intent);
	level.map_trainer.ghost_enabled = CvarToBool(trainer_ghost);
	level.map_trainer.ghost_timings_enabled = CvarToBool(trainer_ghost_timings);
	level.map_trainer.ghost_skill = trainer_ghost_skill ? trainer_ghost_skill->integer : 0;
	level.map_trainer.ghost_body_level = trainer_ghost_body ? trainer_ghost_body->integer : 2;
	level.map_trainer.ghost_adaptive = CvarToBool(trainer_ghost_adaptive);
	level.map_trainer.hud_level = trainer_hud_level ? trainer_hud_level->integer : 3;
	level.map_trainer.match_length_sec = trainer_match_length ? trainer_match_length->integer : 600;
	level.map_trainer.silent_feedback = CvarToBool(trainer_silent_feedback);

	MapTrainer_ValidateTrainerMode();

	const int32_t challenge = static_cast<int32_t>(level.map_trainer.timing_challenge_mode);
	if (challenge < static_cast<int32_t>(timing_challenge_mode_t::OFF) ||
		challenge >= trainer_config::TIMING_CHALLENGE_MODE_COUNT)
	{
		level.map_trainer.timing_challenge_mode = timing_challenge_mode_t::OFF;
	}
}

void MapTrainer_WriteCvars()
{
	CvarSetBool("trainer_weapons", level.map_trainer.weapons_enabled);
	CvarSetBool("trainer_ammo", level.map_trainer.ammo_enabled);
	CvarSetBool("trainer_health", level.map_trainer.health_enabled);
	CvarSetBool("trainer_armor", level.map_trainer.armor_enabled);
	CvarSetBool("trainer_powerups", level.map_trainer.powerups_enabled);
	CvarSetBool("trainer_speedometer", level.map_trainer.speedometer_enabled);
	gi.cvar_set("trainer_mode", G_Fmt("{}", static_cast<int32_t>(level.map_trainer.trainer_mode)).data());
	CvarSetBool("trainer_combine_health", level.map_trainer.combine_health_packs);
	CvarSetBool("trainer_path_stimpacks", level.map_trainer.path_stimpacks_enabled);
	CvarSetBool("trainer_path_armor_shards", level.map_trainer.path_armor_shards_enabled);
	CvarSetBool("trainer_free_collect", level.map_trainer.free_collect_enabled);
	CvarSetBool("trainer_timing_major_only", level.map_trainer.timing_major_items_only);
	gi.cvar_set("trainer_timing_challenge",
		G_Fmt("{}", static_cast<int32_t>(level.map_trainer.timing_challenge_mode)).data());
	CvarSetBool("trainer_bhop", level.map_trainer.bhop_enabled);
	CvarSetBool("trainer_spawn_random", level.map_trainer.spawn_trainer_true_random);
	CvarSetBool("trainer_spawn_beacon", level.map_trainer.spawn_trainer_beacon_enabled);
	CvarSetBool("trainer_spawn_intent", level.map_trainer.spawn_trainer_intent);
	CvarSetBool("trainer_ghost", level.map_trainer.ghost_enabled);
	CvarSetBool("trainer_ghost_timings", level.map_trainer.ghost_timings_enabled);
	gi.cvar_set("trainer_ghost_skill", G_Fmt("{}", level.map_trainer.ghost_skill).data());
	gi.cvar_set("trainer_ghost_body", G_Fmt("{}", level.map_trainer.ghost_body_level).data());
	CvarSetBool("trainer_ghost_adaptive", level.map_trainer.ghost_adaptive);
	gi.cvar_set("trainer_hud_level", G_Fmt("{}", level.map_trainer.hud_level).data());
	gi.cvar_set("trainer_match_length", G_Fmt("{}", level.map_trainer.match_length_sec).data());
	CvarSetBool("trainer_silent_feedback", level.map_trainer.silent_feedback);
}
