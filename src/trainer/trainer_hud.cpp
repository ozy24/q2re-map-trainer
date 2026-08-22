// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include "trainer_config.h"
#include "trainer_logic.h"

// ==================== GHOST DUEL HUD - SERVER SIDE ====================
//
// Packs the trainer's per-frame HUD payload into player stats. The drawing half lives
// in trainer_hud_draw.cpp, which is compiled against the client-game headers.
//
// Everything goes through bit-packed int16_t stat slots rather than svc_layout because
// LAYOUTS_LAYOUT is shared with the TAB scoreboard and the trainer's own PMenu - a
// persistent panel on that channel would fight both.

namespace
{
// One timing entry, reduced to what the HUD needs.
struct hud_timer_t
{
	trainer_logic::hud_item_id_t id = trainer_logic::hud_item_id_t::NONE;
	float seconds_remaining = 0.0f;
};

constexpr int32_t HUD_TIMER_SLOTS = 4;

trainer_logic::hud_level_t CurrentHudLevel()
{
	int32_t level_value = level.map_trainer.hud_level;
	if (level_value < 0 || level_value > static_cast<int32_t>(trainer_logic::hud_level_t::FULL))
		level_value = 0;

	return static_cast<trainer_logic::hud_level_t>(level_value);
}

// The soonest-returning tracked items, most urgent first. There are up to 32 concurrent
// timings but only four slots, and the ones about to come back are the ones you have to
// act on.
int32_t CollectTimers(hud_timer_t *out, int32_t max_out)
{
	int32_t count = 0;

	for (int32_t i = 0; i < level.map_trainer.MAX_TIMING_ENTRIES; i++)
	{
		const map_trainer_t::timing_entry_t *entry = &level.map_trainer.timing_entries[i];
		if (!entry->active || !entry->item_classname)
			continue;

		const trainer_logic::hud_item_id_t id = trainer_logic::HudItemId(entry->item_classname);
		if (id == trainer_logic::hud_item_id_t::NONE)
			continue;

		// Megahealth in its decay phase has no meaningful countdown yet: the clock has
		// not started. Show it as down with an unknown return rather than a wrong number.
		float remaining = 0.0f;
		if (!entry->is_megahealth || entry->megahealth_decay_finished)
			remaining = (MapTrainer_EntryRespawnTime(entry) - level.time).seconds();

		if (remaining < 0.0f)
			remaining = 0.0f;

		// Insertion sort into the top-N by urgency; N is 4, so this is cheaper than
		// sorting all 32.
		int32_t slot = count;
		while (slot > 0 && out[slot - 1].seconds_remaining > remaining)
		{
			if (slot < max_out)
				out[slot] = out[slot - 1];
			slot--;
		}

		if (slot < max_out)
		{
			out[slot].id = id;
			out[slot].seconds_remaining = remaining;
			if (count < max_out)
				count++;
		}
	}

	return count;
}
} // namespace

void MapTrainer_StartMatchClock()
{
	if (level.map_trainer.match_length_sec <= 0)
	{
		level.map_trainer.match_clock_running = false;
		level.map_trainer.match_clock_end = 0_ms;
		return;
	}

	level.map_trainer.match_clock_end =
		level.time + gtime_t::from_sec(level.map_trainer.match_length_sec);
	level.map_trainer.match_clock_running = true;
}

void MapTrainer_StopMatchClock()
{
	level.map_trainer.match_clock_running = false;
	level.map_trainer.match_clock_end = 0_ms;
}

int32_t MapTrainer_MatchClockSecondsLeft()
{
	if (!level.map_trainer.match_clock_running)
		return 0;

	const float remaining = (level.map_trainer.match_clock_end - level.time).seconds();
	if (remaining <= 0.0f)
		return 0;

	return static_cast<int32_t>(remaining);
}

// [Map Trainer] Called once per player frame, alongside the speedometer update.
void MapTrainer_UpdateHudStats(edict_t *player)
{
	if (!player || !player->client)
		return;

	player_state_t &ps = player->client->ps;

	const trainer_logic::hud_level_t hud_level = CurrentHudLevel();

	// The panel is driven by the timing trainer, not by the ghost - tracked respawns are
	// worth seeing during plain timing practice too. The ghost only adds the control
	// readout, which is flagged separately so the client can omit it.
	const bool ghost_active = MapTrainer_IsGhostActive();

	// Off, or nothing to report: clear every slot so a stale panel cannot linger.
	if (hud_level == trainer_logic::hud_level_t::OFF ||
		!MapTrainer_IsModeActive(trainer_mode_t::TIMING))
	{
		ps.stats[STAT_TRAINER_CLOCK] = 0;
		ps.stats[STAT_TRAINER_CONTROL] = 0;
		for (int32_t i = 0; i < HUD_TIMER_SLOTS; i++)
			ps.stats[STAT_TRAINER_TIMER_1 + i] = 0;
		return;
	}

	ps.stats[STAT_TRAINER_CLOCK] = static_cast<int16_t>(MapTrainer_MatchClockSecondsLeft());

	const int32_t ghost_stack = MapTrainer_GhostStack();
	const trainer_logic::control_state_t state = trainer_logic::ControlState(
		MapTrainer_PlayerStack(player), ghost_stack, trainer_config::CONTROL_DEADBAND_STACK);

	ps.stats[STAT_TRAINER_CONTROL] = static_cast<int16_t>(
		trainer_logic::PackControlStat(hud_level, state, ghost_stack, ghost_active));

	hud_timer_t timers[HUD_TIMER_SLOTS];
	const int32_t timer_count = (hud_level >= trainer_logic::hud_level_t::NAMES)
		? CollectTimers(timers, HUD_TIMER_SLOTS) : 0;

	for (int32_t i = 0; i < HUD_TIMER_SLOTS; i++)
	{
		if (i >= timer_count)
		{
			ps.stats[STAT_TRAINER_TIMER_1 + i] = 0;
			continue;
		}

		// At NAMES level the countdown is deliberately withheld - you are told the item
		// is down, and you supply the number.
		const int32_t tenths = (hud_level == trainer_logic::hud_level_t::FULL)
			? static_cast<int32_t>(timers[i].seconds_remaining * 10.0f) : 0;

		ps.stats[STAT_TRAINER_TIMER_1 + i] = static_cast<int16_t>(
			trainer_logic::PackTimerStat(timers[i].id, tenths));
	}
}
