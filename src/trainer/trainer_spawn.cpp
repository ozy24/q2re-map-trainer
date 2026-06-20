// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include "trainer_config.h"

extern edict_t *ClientChooseSlot(const char *userinfo, const char *social_id, bool isBot, edict_t **ignore, size_t num_ignore, bool cinematic);
extern bool ClientConnect(edict_t *ent, char *userinfo, const char *social_id, bool isBot);
extern void ClientBegin(edict_t *ent);
extern void ClientDisconnect(edict_t *ent);

namespace
{
constexpr const char *SPAWN_TRAINER_SOCIAL_ID = "spawn_trainer_bot";
constexpr const char *SPAWN_TRAINER_NAME = "Spawn Trainer";
constexpr const char *SPAWN_TRAINER_SKIN = "male/grunt";
constexpr const char *SPAWN_TRAINER_BEACON_SOUND = "weapons/hyprbf1a.wav";
constexpr gtime_t SPAWN_TRAINER_BEACON_INTERVAL = gtime_t::from_sec(trainer_config::SPAWN_TRAINER_BEACON_INTERVAL_SEC);
constexpr gtime_t SPAWN_TRAINER_BEACON_INITIAL_DELAY = gtime_t::from_sec(trainer_config::SPAWN_TRAINER_BEACON_INITIAL_DELAY_SEC);

int spawn_trainer_beacon_sound_index = 0;

void MapTrainer_ResetBeaconTimer(const gtime_t &delay = SPAWN_TRAINER_BEACON_INTERVAL)
{
	level.map_trainer.spawn_trainer_next_beep_time = level.time + delay;
}

void MapTrainer_PrimeBeacon()
{
	if (spawn_trainer_beacon_sound_index == 0)
		spawn_trainer_beacon_sound_index = gi.soundindex(SPAWN_TRAINER_BEACON_SOUND);
}

bool MapTrainer_SnapSpawnTrainerToGround(edict_t *bot)
{
	contents_t mask = G_GetClipMask(bot);
	vec3_t end = bot->s.origin;
	end[2] -= 256.0f;

	trace_t tr = gi.trace(bot->s.origin, bot->mins, bot->maxs, end, bot, mask);
	if (tr.fraction >= 1.0f)
		return false;

	bot->s.origin = tr.endpos;
	bot->client->ps.pmove.origin = tr.endpos;
	bot->groundentity = tr.ent;
	bot->groundentity_linkcount = tr.ent ? tr.ent->linkcount : 0;
	bot->velocity = vec3_origin;
	bot->avelocity = vec3_origin;
	bot->client->ps.pmove.velocity = vec3_origin;
	gi.linkentity(bot);
	return true;
}

bool MapTrainer_IsSpawnTrainerClient(const edict_t *ent)
{
	if (!ent || !ent->client)
		return false;

	if (!(ent->svflags & SVF_BOT))
		return false;

	return ent->client->pers.social_id[0] &&
		(Q_strcasecmp(ent->client->pers.social_id, SPAWN_TRAINER_SOCIAL_ID) == 0);
}

void MapTrainer_ClearSpawnTrainerState()
{
	level.map_trainer.spawn_trainer_enabled = false;
	level.map_trainer.spawn_trainer_bot = nullptr;
	level.map_trainer.spawn_trainer_owner = nullptr;
	level.map_trainer.spawn_trainer_force_random_pick = false;
	level.map_trainer.spawn_trainer_total_spawns = 0;
	level.map_trainer.spawn_trainer_last_spawn_index = -1;
	level.map_trainer.spawn_trainer_next_beep_time = gtime_t();
}

void MapTrainer_PrepareBotUserinfo(char *userinfo, size_t size)
{
	if (!userinfo || !size)
		return;

	userinfo[0] = '\0';
	gi.Info_SetValueForKey(userinfo, "name", SPAWN_TRAINER_NAME);
	gi.Info_SetValueForKey(userinfo, "skin", SPAWN_TRAINER_SKIN);
	gi.Info_SetValueForKey(userinfo, "hand", "2");
	gi.Info_SetValueForKey(userinfo, "spectator", "0");
	gi.Info_SetValueForKey(userinfo, "gender", "male");
	gi.Info_SetValueForKey(userinfo, "fov", "90");
}
}

void MapTrainer_InitSpawnTrainerState()
{
	MapTrainer_ClearSpawnTrainerState();
	level.map_trainer.spawn_trainer_beacon_enabled = true;
	level.map_trainer.spawn_trainer_next_beep_time = gtime_t();
	spawn_trainer_beacon_sound_index = 0;
	MapTrainer_PrimeBeacon();
}

bool MapTrainer_EnableSpawnTrainer(edict_t *requester)
{
	if (level.map_trainer.spawn_trainer_enabled && level.map_trainer.spawn_trainer_bot &&
		level.map_trainer.spawn_trainer_bot->inuse)
	{
		if (requester)
			gi.LocClient_Print(requester, PRINT_HIGH, "Spawn Trainer already enabled.");
		return true;
	}

	char userinfo[MAX_INFO_STRING] = { 0 };
	MapTrainer_PrepareBotUserinfo(userinfo, sizeof(userinfo));

	edict_t *slot = ClientChooseSlot(userinfo, SPAWN_TRAINER_SOCIAL_ID, true, nullptr, 0, false);
	if (!slot)
	{
		if (requester)
			gi.LocClient_Print(requester, PRINT_HIGH, "No available client slot for Spawn Trainer.");
		MapTrainer_ClearSpawnTrainerState();
		return false;
	}

	if (!ClientConnect(slot, userinfo, SPAWN_TRAINER_SOCIAL_ID, true))
	{
		if (requester)
			gi.LocClient_Print(requester, PRINT_HIGH, "Spawn Trainer bot failed to connect.");
		MapTrainer_ClearSpawnTrainerState();
		return false;
	}

	ClientBegin(slot);
	MapTrainer_OnSpawnTrainerClientBegin(slot);
	level.map_trainer.spawn_trainer_owner = (requester && requester->client) ? requester : nullptr;
	if (level.map_trainer.spawn_trainer_beacon_enabled)
		MapTrainer_ResetBeaconTimer(SPAWN_TRAINER_BEACON_INITIAL_DELAY);
	else
		level.map_trainer.spawn_trainer_next_beep_time = gtime_t();

	return level.map_trainer.spawn_trainer_enabled && level.map_trainer.spawn_trainer_bot;
}

void MapTrainer_DisableSpawnTrainer()
{
	if (!level.map_trainer.spawn_trainer_enabled && !level.map_trainer.spawn_trainer_bot)
		return;

	edict_t *bot = level.map_trainer.spawn_trainer_bot;

	MapTrainer_ClearSpawnTrainerState();

	if (bot && bot->client && bot->inuse)
		ClientDisconnect(bot);
}

bool MapTrainer_HandleSpawnTrainerBot(edict_t *bot)
{
	if (!bot || !bot->client)
		return false;

	if (!level.map_trainer.spawn_trainer_enabled || bot != level.map_trainer.spawn_trainer_bot)
		return false;

	if (!bot->inuse || !MapTrainer_IsSpawnTrainerClient(bot))
	{
		MapTrainer_ClearSpawnTrainerState();
		return false;
	}

	if (bot->deadflag)
	{
		bot->client->latched_buttons |= BUTTON_ATTACK;
		return true;
	}

	if (level.map_trainer.spawn_trainer_beacon_enabled)
	{
		if (level.map_trainer.spawn_trainer_next_beep_time <= gtime_t())
			MapTrainer_ResetBeaconTimer(SPAWN_TRAINER_BEACON_INITIAL_DELAY);

		if (level.time >= level.map_trainer.spawn_trainer_next_beep_time)
		{
			MapTrainer_PrimeBeacon();
			if (spawn_trainer_beacon_sound_index != 0)
				gi.sound(bot, CHAN_AUTO, spawn_trainer_beacon_sound_index, trainer_config::SOUND_VOLUME_FULL, ATTN_IDLE, 0);

			MapTrainer_ResetBeaconTimer();
		}
	}
	else
	{
		level.map_trainer.spawn_trainer_next_beep_time = gtime_t();
	}

	return true;
}

void MapTrainer_OnSpawnTrainerClientBegin(edict_t *ent)
{
	if (!MapTrainer_IsSpawnTrainerClient(ent))
		return;

	level.map_trainer.spawn_trainer_bot = ent;
	level.map_trainer.spawn_trainer_enabled = true;
}

void MapTrainer_OnSpawnTrainerClientDisconnect(edict_t *ent)
{
	if (!MapTrainer_IsSpawnTrainerClient(ent))
		return;

	MapTrainer_ClearSpawnTrainerState();
}

bool MapTrainer_IsSpawnTrainerBot(const edict_t *ent)
{
	return MapTrainer_IsSpawnTrainerClient(ent);
}

void MapTrainer_OnSpawnTrainerRespawn(int32_t total_spawns, int32_t spawn_index)
{
	if (!level.map_trainer.spawn_trainer_enabled)
		return;

	level.map_trainer.spawn_trainer_total_spawns = total_spawns;
	level.map_trainer.spawn_trainer_last_spawn_index = spawn_index;
	if (level.map_trainer.spawn_trainer_bot && level.map_trainer.spawn_trainer_bot->inuse)
		MapTrainer_SnapSpawnTrainerToGround(level.map_trainer.spawn_trainer_bot);
	if (level.map_trainer.spawn_trainer_beacon_enabled)
		MapTrainer_ResetBeaconTimer(SPAWN_TRAINER_BEACON_INITIAL_DELAY);
	else
		level.map_trainer.spawn_trainer_next_beep_time = gtime_t();
}

// Per-frame trainer tick. Handles deferred spawn-trainer auto-resume after a map change:
// the bot can't be created during SpawnEntities (ClientConnect is unsafe there), so
// MapTrainer_Init sets spawn_trainer_resume_pending and we create the bot here on a normal
// server frame, once at least one human client is actually in the game.
void MapTrainer_RunFrame()
{
	if (!level.map_trainer.spawn_trainer_resume_pending)
		return;

	// Already running? Nothing to resume.
	if (level.map_trainer.spawn_trainer_enabled && level.map_trainer.spawn_trainer_bot &&
		level.map_trainer.spawn_trainer_bot->inuse)
	{
		level.map_trainer.spawn_trainer_resume_pending = false;
		return;
	}

	// Wait until a human player is in-game before spawning the bot.
	bool human_present = false;
	for (uint32_t i = 0; i < game.maxclients; i++)
	{
		edict_t *player = &g_edicts[1 + i];
		if (player->inuse && player->client && !(player->svflags & SVF_BOT))
		{
			human_present = true;
			break;
		}
	}

	if (!human_present)
		return; // keep pending until someone loads in

	if (level.time < level.map_trainer.spawn_trainer_resume_next_try)
		return;

	if (MapTrainer_EnableSpawnTrainer(nullptr))
		level.map_trainer.spawn_trainer_resume_pending = false;
	else
		level.map_trainer.spawn_trainer_resume_next_try = level.time + gtime_t::from_sec(trainer_config::SPAWN_TRAINER_RESUME_RETRY_SEC);
}

