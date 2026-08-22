// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include "trainer_config.h"
#include "trainer_logic.h"

// m_player.h declares an unqualified FRAME_* enum that collides with every monster's frame
// enum, which is why g_misc.cpp includes it mid-file. Keeping it to this one dedicated
// translation unit sidesteps the problem entirely.
#include "../m_player.h"

// ==================== GHOST BODY ====================
//
// A visible silhouette for the Ghost Duel opponent.
//
// Without it, the ghost is inferred purely from items vanishing, which makes implausible
// movement invisible until it manifests as "it took two items at once". With it, the
// ghost's route is legible: you can watch it take the real path around corners, which both
// verifies the travel times and teaches you how an opponent actually moves the map.
//
// It is a pure visual: SOLID_NOT, no AI, no client slot, no collision, no damage. It never
// interacts with anything - all gameplay effect still comes from the bodiless simulation
// in trainer_ghost.cpp.
//
// Modelled on SP_misc_player_mannequin (g_misc.cpp:2494) and the target_camera dummy
// (g_target.cpp:1292), with three deltas: SOLID_NOT rather than SOLID_BBOX, a fixed alpha
// rather than the hologram's per-frame flicker, and a run cycle rather than a stand loop.

namespace
{
constexpr int32_t GHOST_RUN_FRAME_COUNT = (FRAME_run6 - FRAME_run1) + 1;

// Find a connected human to borrow a skin from. MODELINDEX_PLAYER resolves the model from
// CS_PLAYERSKINS + skinnum, and a live player's configstring is already written by
// ClientUserinfoChanged - so cloning it means the body needs NO precache at all.
edict_t *GhostBodyFindSkinSource()
{
	for (uint32_t i = 0; i < game.maxclients; i++)
	{
		edict_t *player = &g_edicts[1 + i];
		if (player->inuse && player->client && !(player->svflags & SVF_BOT))
			return player;
	}

	return nullptr;
}

int32_t GhostBodyLevel()
{
	int32_t body_level = level.map_trainer.ghost_body_level;
	if (body_level < 0 || body_level > trainer_config::GHOST_BODY_LEVEL_MAX)
		body_level = 0;
	return body_level;
}
} // namespace

void MapTrainer_GhostDestroyBody()
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	if (ghost.body && ghost.body->inuse)
		G_FreeEdict(ghost.body);

	ghost.body = nullptr;
}

void MapTrainer_GhostCreateBody()
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	if (ghost.body && ghost.body->inuse)
		return;

	edict_t *skin_source = GhostBodyFindSkinSource();
	if (!skin_source)
		return; // no human in-game yet; MapTrainer_GhostUpdateBody retries next frame

	edict_t *body = G_Spawn();
	if (!body)
		return;

	body->classname = "trainer_ghost_body";
	body->movetype = MOVETYPE_NONE; // we set the origin ourselves every frame
	body->solid = SOLID_NOT;        // truly non-colliding (the mannequin uses SOLID_BBOX)
	body->clipmask = CONTENTS_NONE;
	body->takedamage = false;
	body->mins = { -16, -16, -24 };
	body->maxs = { 16, 16, 32 };

	body->s.modelindex = MODELINDEX_PLAYER;
	body->s.modelindex2 = 0; // unarmed silhouette; avoids a weapon-model precache
	body->s.skinnum = skin_source->s.skinnum;
	body->s.effects = EF_HOLOGRAM;
	body->s.renderfx = RF_TRANSLUCENT | RF_FULLBRIGHT | RF_NOSHADOW | RF_NO_LOD;
	// A zero alpha means "default", NOT invisible (game.h:1397) - always set it explicitly.
	body->s.alpha = trainer_config::GHOST_BODY_ALPHA;
	body->s.scale = 1.0f;
	body->s.frame = FRAME_run1;

	body->s.origin = ghost.position;
	body->s.old_origin = ghost.position;

	ghost.body = body;
	ghost.body_frame_time = level.time;

	gi.linkentity(body);

	TrainerLog("GHOSTBODY", "spawned (skin %d, level %d)", body->s.skinnum, GhostBodyLevel());
}

void MapTrainer_GhostUpdateBody()
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;
	const int32_t body_level = GhostBodyLevel();

	if (!ghost.active || body_level <= 0)
	{
		MapTrainer_GhostDestroyBody();
		return;
	}

	if (!ghost.body || !ghost.body->inuse)
	{
		MapTrainer_GhostCreateBody();
		if (!ghost.body)
			return;
	}

	edict_t *body = ghost.body;

	// Level 2 forces the body through walls; level 1 lets normal PVS culling hide it, so
	// you only glimpse it where you would really see an opponent.
	if (body_level >= trainer_config::GHOST_BODY_LEVEL_SEE_THROUGH)
		body->svflags |= SVF_NOCULL;
	else
		body->svflags &= ~SVF_NOCULL;

	body->s.origin = ghost.position;

	// Face the direction of travel. Sampling slightly ahead on the leg gives a stable
	// heading; standing still keeps the previous angle rather than snapping to zero.
	if (ghost.target_ent)
	{
		const float elapsed = (level.time - ghost.depart_time).seconds();
		const vec3_t ahead = MapTrainer_GhostLegPosition(elapsed + 0.2f);
		const vec3_t dir = ahead - ghost.position;

		if (dir.lengthSquared() > 1.0f)
			body->s.angles[YAW] = vectoyaw(dir);
	}

	// Run cycle at 10Hz, matching how vanilla drives player animation (p_view.cpp:1121).
	if (level.time >= ghost.body_frame_time)
	{
		int32_t frame = body->s.frame + 1;
		if (frame > FRAME_run6 || frame < FRAME_run1)
			frame = FRAME_run1;
		body->s.frame = frame;
		ghost.body_frame_time = level.time + 10_hz;
	}

	// Mandatory: an entity that moves without relinking keeps its stale PVS cluster and
	// will freeze or pop for anyone in another area (game.h:1930).
	gi.linkentity(body);
}
