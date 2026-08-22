// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include "trainer_config.h"
#include "trainer_logic.h"

// ==================== GHOST DUEL OPPONENT ====================
//
// A bodiless simulated duel opponent. It cycles the map's major items, genuinely
// consumes them through the vanilla respawn path, and is audible at the item's real
// position when it does.
//
// What it deliberately is NOT: it has no edict, no model, no nav mesh and no combat.
// The Q2RE SDK ships empty bot AI (src/bots/bot_think.cpp) and engine-side navigation
// that needs per-map .nav files, so a real duelling bot is not available to a mod.
// It is also not needed here: what makes duel item control hard to practise is the
// opponent's CONSEQUENCES - items vanishing when you did not take them, and having to
// re-derive the cycle from a sound - not the opponent's body.
//
// Its health/armor are a resource LEDGER used to drive the in-control/out-of-control
// readout. The ghost never takes damage.

namespace
{
// Arrival jitter by skill level. Index 0 is a metronome; higher levels arrive sloppily,
// which is more human but makes it harder to tell your error from the ghost's.
constexpr float GHOST_SKILL_JITTER_SEC[] = { 0.0f, 1.0f, 2.0f, 4.0f };
constexpr int32_t GHOST_SKILL_COUNT = trainer_config::GHOST_SKILL_LEVEL_COUNT;
static_assert(std::size(GHOST_SKILL_JITTER_SEC) == static_cast<size_t>(GHOST_SKILL_COUNT),
	"jitter table must cover every skill level the menu can cycle to");

constexpr int32_t GHOST_HEALTH_START = 100;
constexpr int32_t GHOST_HEALTH_BASE = 100; // megahealth decays back down to this
constexpr int32_t GHOST_HEALTH_MAX = 250;

constexpr int32_t GHOST_MAX_CANDIDATES = 64;

edict_t *GhostFindPlayer(); // defined below; needed by the routing scorer

// An item is there to be taken only when it is solid and not mid-respawn. Checking
// solid as well as SVF_RESPAWNING also covers megahealth, which vanilla hides via
// MegaHealth_think without ever setting SVF_RESPAWNING.
bool GhostItemAvailable(const edict_t *ent)
{
	return ent && ent->inuse && ent->item && ent->item->classname &&
		ent->solid != SOLID_NOT && !(ent->svflags & SVF_RESPAWNING);
}

// Seconds until this item is takeable again.
// Returns false when the item is hidden on a schedule we cannot read, in which case
// the ghost simply does not consider it this cycle.
bool GhostSecondsUntilAvailable(const edict_t *ent, float *out_seconds)
{
	if (!ent || !ent->inuse || !ent->item || !ent->item->classname)
		return false;

	if (GhostItemAvailable(ent))
	{
		*out_seconds = 0.0f;
		return true;
	}

	if ((ent->svflags & SVF_RESPAWNING) && ent->nextthink > level.time)
	{
		*out_seconds = (ent->nextthink - level.time).seconds();
		return true;
	}

	return false;
}

int32_t GhostArmorCap(const char *classname)
{
	if (Q_strcasecmp(classname, "item_armor_body") == 0)
		return 200;
	if (Q_strcasecmp(classname, "item_armor_combat") == 0)
		return 150;
	if (Q_strcasecmp(classname, "item_armor_jacket") == 0)
		return 100;
	return 0;
}

int32_t GhostArmorBonus(const char *classname)
{
	if (Q_strcasecmp(classname, "item_armor_body") == 0)
		return 100;
	if (Q_strcasecmp(classname, "item_armor_combat") == 0)
		return 50;
	if (Q_strcasecmp(classname, "item_armor_jacket") == 0)
		return 25;
	return 0;
}

// Credit the ghost's ledger for an item it just took.
void GhostAccrueItem(const char *classname)
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	if (Q_strcasecmp(classname, "item_health_mega") == 0)
	{
		ghost.health = min(ghost.health + 100, GHOST_HEALTH_MAX);
		ghost.next_decay_time = level.time + 1_sec;
		return;
	}

	const int32_t bonus = GhostArmorBonus(classname);
	if (bonus > 0)
	{
		// A lower-tier armor never drags the ghost's cap down below what it already has.
		const int32_t cap = max(ghost.armor, GhostArmorCap(classname));
		ghost.armor = min(ghost.armor + bonus, cap);
	}
}

// How long the item stays gone. Megahealth is special: it only starts its 20s respawn
// once the holder's health has decayed back to base, so a stacked ghost keeps it off
// the map far longer - exactly as a stacked human would.
gtime_t GhostRespawnDelay(const char *classname, gtime_t base_respawn)
{
	if (Q_strcasecmp(classname, "item_health_mega") == 0)
	{
		const map_trainer_ghost_t &ghost = level.map_trainer.ghost;
		const int32_t excess = ghost.health > GHOST_HEALTH_BASE ? ghost.health - GHOST_HEALTH_BASE : 0;
		return gtime_t::from_sec(excess) + base_respawn;
	}

	return base_respawn;
}

float GhostSkillJitterSeconds()
{
	int32_t skill = level.map_trainer.ghost_skill;
	if (skill < 0 || skill >= GHOST_SKILL_COUNT)
		skill = 0;

	const float jitter = GHOST_SKILL_JITTER_SEC[skill];
	if (jitter <= 0.0f)
		return 0.0f;

	return crandom() * jitter;
}

// Choose the next item to route to and commit an arrival time.
// Enumerates live entities rather than reading level.map_trainer.items, because that
// array belongs to path mode and is freed when path mode is disabled.
void GhostPickTarget()
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	trainer_logic::ghost_candidate_t candidates[GHOST_MAX_CANDIDATES];
	edict_t *candidate_ents[GHOST_MAX_CANDIDATES];
	int32_t count = 0;

	edict_t *player = GhostFindPlayer();

	for (uint32_t i = 1; i < globals.num_edicts && count < GHOST_MAX_CANDIDATES; i++)
	{
		edict_t *ent = &g_edicts[i];
		if (!ent->inuse || !ent->item || !ent->item->classname)
			continue;

		const int32_t value = trainer_logic::GhostItemValue(ent->item->classname);
		if (value <= 0)
			continue;

		float respawn_in = 0.0f;
		if (!GhostSecondsUntilAvailable(ent, &respawn_in))
			continue;

		// Scoring pass: cached real distance where we have one, cheap estimate otherwise.
		// Paying for a pathfind per candidate would cost N queries per routing decision.
		candidates[count].item_index = count;
		candidates[count].distance = MapTrainer_GhostRouteDistance(
			ghost.position_ent, ghost.position, ent, ent->s.origin, false);
		candidates[count].respawn_in_sec = respawn_in;
		candidates[count].value = value;
		candidates[count].player_distance = player
			? MapTrainer_GhostEstimateDistance(player->s.origin, ent->s.origin)
			: trainer_config::GHOST_PLAYER_AVOID_RANGE; // no player: nothing to avoid
		candidate_ents[count] = ent;
		count++;
	}

	// Play the same in-control / out-of-control game the trainer measures the player on.
	trainer_logic::ghost_policy_t policy =
		trainer_logic::GhostPolicyForControl(trainer_logic::control_state_t::EVEN);
	if (level.map_trainer.ghost_adaptive && player)
	{
		// Note the argument order: ControlState(first, second) reports how the FIRST party
		// is doing, so passing the ghost's stack first yields the ghost's own control
		// state - IN means the ghost is winning, which is what drives its policy.
		policy = trainer_logic::GhostPolicyForControl(trainer_logic::ControlState(
			MapTrainer_GhostStack(), MapTrainer_PlayerStack(player),
			trainer_config::CONTROL_DEADBAND_STACK));
	}

	const int32_t pick = trainer_logic::PickGhostTarget(candidates, count,
		trainer_config::GHOST_SPEED_UPS, trainer_config::GHOST_PATH_INEFFICIENCY, policy);

	if (pick < 0)
	{
		// Nothing routable right now (every major hidden on an unreadable schedule).
		// Idle and retry next frame.
		ghost.target_ent = nullptr;
		return;
	}

	edict_t *target = candidate_ents[pick];

	// Commit pass: build the real route for the chosen target. This is the one pathfind we
	// pay for per routing decision, and it populates the distance cache for future scoring.
	const bool measured = MapTrainer_GhostBuildLeg(ghost.position, target->s.origin);

	float travel;
	float route_distance;

	if (measured)
	{
		// Speed came from the route's own geometry, so the duration is already the answer.
		travel = ghost.leg_duration;
		route_distance = MapTrainer_GhostRouteDistance(
			ghost.position_ent, ghost.position, target, target->s.origin, false);
	}
	else
	{
		route_distance = MapTrainer_GhostRouteDistance(
			ghost.position_ent, ghost.position, target, target->s.origin, false);
		travel = trainer_logic::EstimateTravelSeconds(route_distance,
			trainer_config::GHOST_SPEED_UPS, trainer_config::GHOST_PATH_INEFFICIENCY);
	}

	if (travel < trainer_config::GHOST_MIN_TRAVEL_SEC)
		travel = trainer_config::GHOST_MIN_TRAVEL_SEC;

	// The ghost cannot take an item before it exists.
	float arrive_in = max(travel, candidates[pick].respawn_in_sec);
	arrive_in += GhostSkillJitterSeconds();
	if (arrive_in < 0.0f)
		arrive_in = 0.0f;

	ghost.target_ent = target;
	ghost.target_position = target->s.origin;
	ghost.route_start = ghost.position;
	ghost.depart_time = level.time;
	ghost.arrive_time = level.time + gtime_t::from_sec(arrive_in);

	// Never pick up two items in quick succession, however close together they are.
	if (ghost.arrive_time < ghost.next_pickup_allowed)
		ghost.arrive_time = ghost.next_pickup_allowed;

	TrainerLog("GHOST", "route -> %s dist %.0f (%s) travel %.1fs respawn_in %.1fs arrive in %.1fs",
		target->item->classname, route_distance, measured ? "nav" : "estimate", travel,
		candidates[pick].respawn_in_sec, (ghost.arrive_time - level.time).seconds());
}

// The ghost reaches its target and takes it.
void GhostConsumeTarget()
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;
	edict_t *ent = ghost.target_ent;

	// Beaten to it, or the item went away for some other reason. Re-route.
	if (!GhostItemAvailable(ent))
	{
		ghost.target_ent = nullptr;
		return;
	}

	// The ghost's arrival is a scheduled claim, not a physical presence - nothing stops
	// it landing on the exact tick the item respawns, before the player's own touch for
	// that tick could ever see it solid. If the player is already standing on it, that is
	// a real steal, not a fair race: back off and re-route instead of taking it anyway.
	edict_t *player = GhostFindPlayer();
	if (player && (player->s.origin - ent->s.origin).length() <= trainer_config::TIMING_ITEM_PICKUP_RADIUS)
	{
		ghost.target_ent = nullptr;
		return;
	}

	const char *classname = ent->item->classname;
	const char *item_name = nullptr;
	gtime_t base_respawn = 20_sec;

	if (!MapTrainer_GetTimedItemInfo(classname, ent->item, &item_name, &base_respawn))
	{
		ghost.target_ent = nullptr;
		return;
	}

	// Kill any timing the player had running on this item before starting a new one -
	// the cycle they were tracking no longer exists.
	map_trainer_t::timing_entry_t *existing = MapTrainer_FindTimingEntry(classname, ent->s.origin);
	if (existing && existing->active)
	{
		MapTrainer_NotifyTimingDenied(existing, "ghost");
		ghost.denials_against++;
	}

	// Ledger first: megahealth's respawn delay depends on the ghost's resulting stack.
	GhostAccrueItem(classname);
	const gtime_t delay = GhostRespawnDelay(classname, base_respawn);

	SetRespawn(ent, delay, true);

	// The information leak that real timing is built on: the item's own pickup sound,
	// at the item's own position, attenuated normally. Audible if you are near, silent
	// if you are across the map. Pickup sounds are already precached by the item.
	if (ent->item->pickup_sound)
	{
		gi.positioned_sound(ent->s.origin, ent, CHAN_ITEM, gi.soundindex(ent->item->pickup_sound),
			trainer_config::SOUND_VOLUME_FULL, ATTN_NORM, 0.0f);
	}

	// Start a timing entry attributed to the ghost. The player was told nothing except
	// the sound - returning on time is the whole drill.
	// Gated on TIMING mode as well as the toggle: nothing evaluates or clears entries
	// when the timing trainer is off, so creating them would just exhaust the 32 slots.
	if (level.map_trainer.ghost_timings_enabled &&
		MapTrainer_IsModeActive(trainer_mode_t::TIMING))
	{
		MapTrainer_CreateOrUpdateTimingEntry(classname, item_name, ent->s.origin,
			level.time, delay, ent);
	}

	ghost.position = ent->s.origin;
	ghost.position_ent = ent; // now standing on a known node; cache lookups work again
	ghost.route_start = ghost.position;
	ghost.items_taken++;
	ghost.target_ent = nullptr;
	ghost.next_pickup_allowed = level.time +
		gtime_t::from_sec(trainer_config::GHOST_MIN_PICKUP_INTERVAL_SEC);

	TrainerLog("GHOST", "took %s at (%.0f,%.0f,%.0f) back in %.1fs (stack %d/%d)",
		classname, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2],
		delay.seconds(), ghost.health, ghost.armor);
}

// Slide the ghost along its current leg. With a measured route this walks the real path
// around corners and up stairs; without one it degrades to a straight-line lerp.
void GhostUpdatePosition()
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;
	if (!ghost.target_ent)
		return;

	float elapsed = (level.time - ghost.depart_time).seconds();

	// The committed arrival may be later than the route's own duration (waiting for a
	// respawn, or the minimum pickup interval). Stretch the walk to fill the leg rather
	// than arriving early and standing still, which would look like teleporting again.
	const float committed = (ghost.arrive_time - ghost.depart_time).seconds();
	if (ghost.leg_count > 0 && ghost.leg_duration > 0.0f && committed > ghost.leg_duration)
		elapsed *= ghost.leg_duration / committed;

	ghost.position = MapTrainer_GhostLegPosition(elapsed);
}

// First human player in the game. The trainer is single-practitioner by design (see
// README "Design and limitations"), so "the player" is well defined.
edict_t *GhostFindPlayer()
{
	for (uint32_t i = 0; i < game.maxclients; i++)
	{
		edict_t *player = &g_edicts[1 + i];
		if (player->inuse && player->client && !(player->svflags & SVF_BOT))
			return player;
	}

	return nullptr;
}

// Tell the player which playbook they are in, but only when it changes - a readout
// that reprints every frame is noise, and the point is to notice the transition.
void GhostUpdateControlState()
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	edict_t *player = GhostFindPlayer();
	if (!player || player->deadflag)
	{
		// Break the sampling interval while dead. Leaving it running would charge the
		// whole death-to-respawn gap to whichever control state was current when you died.
		MapTrainer_SessionPauseControlSampling();
		return;
	}

	const trainer_logic::control_state_t state = trainer_logic::ControlState(
		MapTrainer_PlayerStack(player), MapTrainer_GhostStack(),
		trainer_config::CONTROL_DEADBAND_STACK);

	const int32_t state_value = static_cast<int32_t>(state);

	// Sampled every frame, not just on change - the summary reports how long you spent
	// in each state, which is the real scoreboard of the resource game.
	MapTrainer_SessionSampleControl(state_value);

	if (ghost.control_state_known && ghost.last_control_state == state_value)
		return;

	const bool first = !ghost.control_state_known;
	ghost.control_state_known = true;
	ghost.last_control_state = state_value;

	// Do not announce the opening position; only announce that it changed.
	if (first)
		return;

	switch (state)
	{
	case trainer_logic::control_state_t::IN:
		gi.LocClient_Print(player, PRINT_CENTER, "IN CONTROL\nExtend the lead - deny the next major");
		break;
	case trainer_logic::control_state_t::OUT:
		gi.LocClient_Print(player, PRINT_CENTER, "OUT OF CONTROL\nSurvive, chip, steal the next cycle");
		break;
	case trainer_logic::control_state_t::EVEN:
		gi.LocClient_Print(player, PRINT_CENTER, "EVEN\nNext major decides it");
		break;
	}

	TrainerLog("GHOST", "control -> %d (player %d vs ghost %d)",
		state_value, MapTrainer_PlayerStack(player), MapTrainer_GhostStack());
}

// Place the ghost where an opponent would actually start: a deathmatch spawn point,
// chosen at random so repeated sessions on the same map do not run an identical script.
//
// Seeding on an ITEM (the original behaviour) was a bug: the ghost scored the item it was
// standing on at distance zero, which made it both the cheapest and the highest-value
// candidate, so it consumed that item within the minimum travel time of being enabled and
// then immediately hopped to a neighbour. That read as "it took two items at once".
void GhostSeedPosition()
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	edict_t *spawns[GHOST_MAX_CANDIDATES];
	int32_t count = 0;

	for (uint32_t i = 1; i < globals.num_edicts && count < GHOST_MAX_CANDIDATES; i++)
	{
		edict_t *ent = &g_edicts[i];
		if (!ent->inuse || !ent->classname)
			continue;
		if (Q_strcasecmp(ent->classname, "info_player_deathmatch") != 0)
			continue;

		spawns[count++] = ent;
	}

	if (count > 0)
	{
		ghost.position = spawns[irandom(count)]->s.origin;
	}
	else
	{
		// No DM spawns (unlikely in deathmatch). Fall back to the world origin rather
		// than to an item, so we never re-introduce the stand-on-the-item bug.
		ghost.position = vec3_origin;
		TrainerLog("GHOST", "no info_player_deathmatch found; seeding at world origin");
	}

	ghost.route_start = ghost.position;
	ghost.position_ent = nullptr; // a spawn point is not a node, so no cache entry applies
	// Do not let the ghost grab anything the instant it is enabled.
	ghost.next_pickup_allowed = level.time +
		gtime_t::from_sec(trainer_config::GHOST_MIN_PICKUP_INTERVAL_SEC);
}
} // namespace

void MapTrainer_InitGhost()
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	ghost = {};
	ghost.target_ent = nullptr;
	ghost.health = GHOST_HEALTH_START;
	ghost.armor = 0;

	if (level.map_trainer.ghost_skill < 0 || level.map_trainer.ghost_skill >= GHOST_SKILL_COUNT)
		level.map_trainer.ghost_skill = 0;
}

bool MapTrainer_IsGhostActive()
{
	return level.map_trainer.ghost.active;
}

int32_t MapTrainer_GhostStack()
{
	const map_trainer_ghost_t &ghost = level.map_trainer.ghost;
	return trainer_logic::StackValue(ghost.health, ghost.armor);
}

int32_t MapTrainer_PlayerStack(const edict_t *player)
{
	if (!player || !player->client)
		return 0;

	// ArmorIndex takes a non-const edict but only reads inventory.
	edict_t *mutable_player = const_cast<edict_t *>(player);
	const item_id_t armor_index = ArmorIndex(mutable_player);
	const int32_t armor = (armor_index != IT_NULL)
		? player->client->pers.inventory[armor_index] : 0;

	return trainer_logic::StackValue(player->health, armor);
}

void MapTrainer_SetGhostEnabled(bool enabled, edict_t *notify)
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	level.map_trainer.ghost_enabled = enabled;

	if (enabled)
	{
		// The ghost's whole output is timing pressure, which is meaningless unless the
		// timing trainer is evaluating. Turn it on rather than silently doing nothing.
		MapTrainer_SetModeFlag(trainer_mode_t::TIMING, true, notify);

		// Ghost Duel is exclusive of the other item/movement trainers. Path Trainer in
		// particular is actively incompatible: its pickup gate (g_items.cpp Touch_Item)
		// blocks the player from taking anything but the current goal item, while the
		// ghost races for any major on the map - running both meant the player could
		// almost never contest what the ghost was going for. Jump/bhop and Spawn Trainer
		// do not conflict mechanically, but Ghost Duel is meant to run alone.
		MapTrainer_SetModeFlag(trainer_mode_t::PATH, false, notify);
		level.map_trainer.bhop_enabled = false;
		if (level.map_trainer.spawn_trainer_enabled)
		{
			MapTrainer_DisableSpawnTrainer();
			level.map_trainer.spawn_trainer_intent = false;
		}

		ghost = {};
		ghost.health = GHOST_HEALTH_START;
		ghost.armor = 0;
		ghost.active = true;
		MapTrainer_GhostBuildNodes();
		GhostSeedPosition();
		MapTrainer_SessionReset();
		level.map_trainer.match_clock_expired = false;
		MapTrainer_StartMatchClock();

		if (notify)
		{
			gi.LocClient_Print(notify, PRINT_HIGH,
				"Ghost Duel enabled. An opponent is now cycling this map's major items.");
			gi.LocClient_Print(notify, PRINT_HIGH,
				"You will hear it take them. Time them by ear.");
		}

		TrainerLog("GHOST", "enabled on %s (skill %d, timings %d)",
			level.mapname, level.map_trainer.ghost_skill,
			level.map_trainer.ghost_timings_enabled ? 1 : 0);
	}
	else
	{
		// Report before tearing down, so a run is never silently discarded.
		if (ghost.active && notify)
		{
			gi.LocClient_Print(notify, PRINT_HIGH, "Ghost Duel disabled.");
			MapTrainer_SessionPrintSummary(notify);
		}

		ghost.active = false;
		ghost.target_ent = nullptr;
		MapTrainer_GhostDestroyBody();
		MapTrainer_StopMatchClock();
	}

	MapTrainer_SaveConfig();
}

bool MapTrainer_GhostOnPlayerPickup(edict_t *item_ent, edict_t *player)
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	if (!ghost.active || !ghost.target_ent || ghost.target_ent != item_ent)
		return false;

	ghost.denials_for++;
	ghost.target_ent = nullptr;

	if (player && player->client)
	{
		const char *name = (item_ent->item && item_ent->item->classname)
			? item_ent->item->classname : "Item";
		gi.LocClient_Print(player, PRINT_HIGH,
			G_Fmt("DENIED the ghost ({}). It is re-routing.", name).data());
	}

	TrainerLog("GHOST", "denied by player; re-routing (denials for %d)", ghost.denials_for);
	return true;
}

void MapTrainer_GhostRunFrame()
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	if (!ghost.active)
		return;

	// Megahealth decay, 1hp/sec back down to base. Drives how long a ghost-taken
	// megahealth stays off the map.
	if (ghost.health > GHOST_HEALTH_BASE && level.time >= ghost.next_decay_time)
	{
		ghost.health--;
		ghost.next_decay_time = level.time + 1_sec;
	}

	GhostUpdateControlState();

	// Match clock expiry: this is where a practice run is consolidated, so the summary
	// fires here whether or not per-attempt feedback was suppressed.
	if (level.map_trainer.match_clock_running && !level.map_trainer.match_clock_expired &&
		level.time >= level.map_trainer.match_clock_end)
	{
		level.map_trainer.match_clock_expired = true;
		MapTrainer_StopMatchClock();
		MapTrainer_SessionPrintSummaryToAll();
		TrainerLog("SESSION", "match clock expired; summary printed");
	}

	// Target went away underneath us (map churn, entity freed).
	if (ghost.target_ent && !ghost.target_ent->inuse)
		ghost.target_ent = nullptr;

	if (!ghost.target_ent)
	{
		GhostPickTarget();
		return;
	}

	if (level.time >= ghost.arrive_time)
	{
		GhostConsumeTarget();
		return;
	}

	GhostUpdatePosition();
	MapTrainer_GhostUpdateBody();
}
