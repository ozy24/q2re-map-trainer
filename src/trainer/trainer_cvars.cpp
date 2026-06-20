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
cvar_t *trainer_free_collect;
cvar_t *trainer_timing_major_only;
cvar_t *trainer_timing_challenge;
cvar_t *trainer_bhop;
cvar_t *trainer_spawn_random;
cvar_t *trainer_spawn_beacon;
cvar_t *trainer_spawn_intent;

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
	trainer_free_collect = gi.cvar("trainer_free_collect", "1", CVAR_ARCHIVE);
	trainer_timing_major_only = gi.cvar("trainer_timing_major_only", "1", CVAR_ARCHIVE);
	trainer_timing_challenge = gi.cvar("trainer_timing_challenge", "0", CVAR_ARCHIVE);
	trainer_bhop = gi.cvar("trainer_bhop", "0", CVAR_ARCHIVE);
	trainer_spawn_random = gi.cvar("trainer_spawn_random", "0", CVAR_ARCHIVE);
	trainer_spawn_beacon = gi.cvar("trainer_spawn_beacon", "1", CVAR_ARCHIVE);
	trainer_spawn_intent = gi.cvar("trainer_spawn_intent", "0", CVAR_ARCHIVE);
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
	level.map_trainer.free_collect_enabled = CvarToBool(trainer_free_collect);
	level.map_trainer.timing_major_items_only = CvarToBool(trainer_timing_major_only);
	level.map_trainer.timing_challenge_mode = static_cast<timing_challenge_mode_t>(
		trainer_timing_challenge ? trainer_timing_challenge->integer : 0);
	level.map_trainer.bhop_enabled = CvarToBool(trainer_bhop);
	level.map_trainer.spawn_trainer_true_random = CvarToBool(trainer_spawn_random);
	level.map_trainer.spawn_trainer_beacon_enabled = CvarToBool(trainer_spawn_beacon);
	level.map_trainer.spawn_trainer_intent = CvarToBool(trainer_spawn_intent);

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
	CvarSetBool("trainer_free_collect", level.map_trainer.free_collect_enabled);
	CvarSetBool("trainer_timing_major_only", level.map_trainer.timing_major_items_only);
	gi.cvar_set("trainer_timing_challenge",
		G_Fmt("{}", static_cast<int32_t>(level.map_trainer.timing_challenge_mode)).data());
	CvarSetBool("trainer_bhop", level.map_trainer.bhop_enabled);
	CvarSetBool("trainer_spawn_random", level.map_trainer.spawn_trainer_true_random);
	CvarSetBool("trainer_spawn_beacon", level.map_trainer.spawn_trainer_beacon_enabled);
	CvarSetBool("trainer_spawn_intent", level.map_trainer.spawn_trainer_intent);
}
