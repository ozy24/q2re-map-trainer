// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include "trainer_config.h"
#include "trainer_logic.h"

// ==================== SESSION SCORING ====================
//
// Per-attempt feedback tells you about one attempt. What actually changes how you time
// is the aggregate: whether you are habitually early or late (a correctable bias), and
// which items you are worst on. That is what this records.
//
// It is deliberately per-map runtime state - a session is one practice run on one map -
// and is reported at match end, on map change, or on demand via "trainer_session".

namespace
{
map_trainer_session_t::item_stat_t *FindOrAddItemStat(trainer_logic::hud_item_id_t id)
{
	map_trainer_session_t &session = level.map_trainer.session;
	const int32_t id_value = static_cast<int32_t>(id);

	for (int32_t i = 0; i < session.item_stat_count; i++)
	{
		if (session.items[i].id == id_value)
			return &session.items[i];
	}

	if (session.item_stat_count >= map_trainer_session_t::MAX_ITEM_STATS)
		return nullptr;

	map_trainer_session_t::item_stat_t *stat = &session.items[session.item_stat_count++];
	stat->id = id_value;
	stat->attempts = 0;
	stat->total_signed_error = 0.0f;
	stat->total_abs_error = 0.0f;
	return stat;
}

// Rolling bhop consistency from the buffer trainer_jump.cpp has always been filling.
// Returns -1 when there is not enough data to say anything.
int32_t BhopConsistencyPercent(const edict_t *player)
{
	if (!player || !player->client)
		return -1;

	const uint8_t count = player->client->bhop_result_window_count;
	if (count == 0)
		return -1;

	int32_t perfect = 0;
	for (uint8_t i = 0; i < count; i++)
	{
		if (player->client->bhop_result_window[i])
			perfect++;
	}

	return (perfect * 100) / count;
}
} // namespace

void MapTrainer_SessionReset()
{
	level.map_trainer.session = {};
	level.map_trainer.session.best_abs_error = -1.0f;
	level.map_trainer.session.worst_abs_error = -1.0f;
}

void MapTrainer_SessionRecordTiming(const char *classname, float signed_error_sec)
{
	map_trainer_session_t &session = level.map_trainer.session;

	const float abs_error = signed_error_sec < 0.0f ? -signed_error_sec : signed_error_sec;

	session.attempts++;
	session.total_signed_error += signed_error_sec;
	session.total_abs_error += abs_error;

	if (session.best_abs_error < 0.0f || abs_error < session.best_abs_error)
		session.best_abs_error = abs_error;
	if (session.worst_abs_error < 0.0f || abs_error > session.worst_abs_error)
		session.worst_abs_error = abs_error;

	// "Early" means arriving before the item is back, which is the safer error in a real
	// duel - you hold the spot. Late means you conceded the cycle.
	if (signed_error_sec < 0.0f)
		session.early_count++;
	else if (signed_error_sec > 0.0f)
		session.late_count++;

	if (level.map_trainer.timing_challenge_mode != timing_challenge_mode_t::OFF)
	{
		session.challenge_attempts++;
		const auto challenge = trainer_logic::EvaluateTimingChallenge(signed_error_sec,
			static_cast<int32_t>(level.map_trainer.timing_challenge_mode));
		if (challenge.success)
			session.within_window++;
	}

	map_trainer_session_t::item_stat_t *stat = FindOrAddItemStat(trainer_logic::HudItemId(classname));
	if (stat)
	{
		stat->attempts++;
		stat->total_signed_error += signed_error_sec;
		stat->total_abs_error += abs_error;
	}

	TrainerLog("SESSION", "attempt %d: %s %+.2f (mean signed %+.2f)",
		session.attempts, classname ? classname : "?", signed_error_sec,
		session.total_signed_error / static_cast<float>(session.attempts));
}

void MapTrainer_SessionPauseControlSampling()
{
	level.map_trainer.session.control_sampling = false;
}

void MapTrainer_SessionSampleControl(int32_t control_state)
{
	map_trainer_session_t &session = level.map_trainer.session;

	if (session.control_sampling)
	{
		const gtime_t delta = level.time - session.last_control_sample;
		if (delta > 0_ms)
		{
			switch (static_cast<trainer_logic::control_state_t>(control_state))
			{
			case trainer_logic::control_state_t::IN: session.time_in_control += delta; break;
			case trainer_logic::control_state_t::OUT: session.time_out_of_control += delta; break;
			case trainer_logic::control_state_t::EVEN:
			default: session.time_even += delta; break;
			}
		}
	}

	session.control_sampling = true;
	session.last_control_sample = level.time;
}

void MapTrainer_SessionPrintSummary(edict_t *player)
{
	if (!player || !player->client)
		return;

	const map_trainer_session_t &session = level.map_trainer.session;
	const map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	gi.LocClient_Print(player, PRINT_HIGH, "\n===== Session summary =====");

	if (session.attempts == 0)
	{
		gi.LocClient_Print(player, PRINT_HIGH, "No timing attempts recorded.");
	}
	else
	{
		const float mean_signed = session.total_signed_error / static_cast<float>(session.attempts);
		const float mean_abs = session.total_abs_error / static_cast<float>(session.attempts);

		// The headline number. A consistent sign is a habit you can correct directly;
		// a mean near zero with a large spread is a different problem entirely.
		const char *bias = "balanced";
		if (mean_signed < -0.5f)
			bias = "habitually EARLY";
		else if (mean_signed > 0.5f)
			bias = "habitually LATE";

		gi.LocClient_Print(player, PRINT_HIGH,
			G_Fmt("Attempts: {}  |  mean error {:+.2f}s ({})", session.attempts, mean_signed, bias).data());
		gi.LocClient_Print(player, PRINT_HIGH,
			G_Fmt("Mean miss {:.2f}s  |  best {:.2f}s  |  worst {:.2f}s",
				mean_abs, session.best_abs_error, session.worst_abs_error).data());
		gi.LocClient_Print(player, PRINT_HIGH,
			G_Fmt("Early {} / Late {}", session.early_count, session.late_count).data());

		if (session.challenge_attempts > 0)
		{
			const int32_t pct = (session.within_window * 100) / session.challenge_attempts;
			gi.LocClient_Print(player, PRINT_HIGH,
				G_Fmt("In window: {}/{} ({}%)", session.within_window, session.challenge_attempts, pct).data());
		}

		for (int32_t i = 0; i < session.item_stat_count; i++)
		{
			const map_trainer_session_t::item_stat_t &stat = session.items[i];
			if (stat.attempts == 0)
				continue;

			const char *name = trainer_logic::HudItemShortName(
				static_cast<trainer_logic::hud_item_id_t>(stat.id));
			gi.LocClient_Print(player, PRINT_HIGH,
				G_Fmt("  {:<5} x{:<3} mean {:+.2f}s  (miss {:.2f}s)", name, stat.attempts,
					stat.total_signed_error / static_cast<float>(stat.attempts),
					stat.total_abs_error / static_cast<float>(stat.attempts)).data());
		}
	}

	// Control-time split: this is the actual scoreboard of a duel's resource game.
	const float total_control = (session.time_in_control + session.time_even +
		session.time_out_of_control).seconds();
	if (total_control > 0.0f)
	{
		gi.LocClient_Print(player, PRINT_HIGH,
			G_Fmt("Control: in {:.0f}% / even {:.0f}% / out {:.0f}%",
				(session.time_in_control.seconds() / total_control) * 100.0f,
				(session.time_even.seconds() / total_control) * 100.0f,
				(session.time_out_of_control.seconds() / total_control) * 100.0f).data());
	}

	if (ghost.items_taken > 0 || ghost.denials_for > 0 || ghost.denials_against > 0)
	{
		gi.LocClient_Print(player, PRINT_HIGH,
			G_Fmt("Ghost took {} items  |  you denied it {}  |  it denied you {}",
				ghost.items_taken, ghost.denials_for, ghost.denials_against).data());
	}

	const int32_t bhop = BhopConsistencyPercent(player);
	if (bhop >= 0)
	{
		gi.LocClient_Print(player, PRINT_HIGH,
			G_Fmt("Bhop consistency (last {} jumps): {}%",
				player->client->bhop_result_window_count, bhop).data());
	}

	gi.LocClient_Print(player, PRINT_HIGH, "===========================\n");
}

void MapTrainer_SessionPrintSummaryToAll()
{
	for (uint32_t i = 0; i < game.maxclients; i++)
	{
		edict_t *player = &g_edicts[1 + i];
		if (player->inuse && player->client && !(player->svflags & SVF_BOT))
			MapTrainer_SessionPrintSummary(player);
	}
}

void Cmd_TrainerSession_f(edict_t *ent)
{
	if (!ent || !ent->client)
		return;

	MapTrainer_SessionPrintSummary(ent);
}
