// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Host-compiled unit tests for trainer_logic (no Quake engine).

#include "trainer_logic.h"
#include "trainer_version.h"

#include <cstdio>
#include <cstring>

namespace
{
int g_failures = 0;

void expect_true(bool value, const char *message)
{
	if (!value)
	{
		std::printf("FAIL: %s\n", message);
		++g_failures;
	}
}

void expect_false(bool value, const char *message)
{
	expect_true(!value, message);
}

void expect_str_eq(const char *actual, const char *expected, const char *message)
{
	if (!actual)
		actual = "";
	if (!expected)
		expected = "";
	if (std::strcmp(actual, expected) != 0)
	{
		std::printf("FAIL: %s (expected '%s', got '%s')\n", message, expected, actual);
		++g_failures;
	}
}

void expect_float_eq(float actual, float expected, const char *message)
{
	if (actual != expected)
	{
		std::printf("FAIL: %s (expected %g, got %g)\n", message, expected, actual);
		++g_failures;
	}
}

void test_combinable_health_packs()
{
	expect_true(trainer_logic::IsCombinableHealthPack("item_health"), "medium health combinable");
	expect_true(trainer_logic::IsCombinableHealthPack("item_health_small"), "small health combinable");
	expect_true(trainer_logic::IsCombinableHealthPack("item_health_large"), "large health combinable");
	expect_false(trainer_logic::IsCombinableHealthPack("item_health_mega"), "mega health not combinable");
}

void test_normalize_class_name()
{
	expect_str_eq(trainer_logic::NormalizeClassName("item_armor_body", false), "item_armor_body",
		"normalize passthrough when combine off");
	expect_str_eq(trainer_logic::NormalizeClassName("item_health", true), "item_health_combined",
		"normalize combines health when enabled");
	expect_str_eq(trainer_logic::NormalizeClassName("weapon_shotgun", true), "weapon_shotgun",
		"normalize leaves non-health alone");
}

void test_category_by_classname()
{
	trainer_logic::category_toggles_t all_on{};
	expect_true(trainer_logic::IsItemCategoryEnabledByClassName("weapon_shotgun", all_on), "weapon on");

	trainer_logic::category_toggles_t weapons_off{};
	weapons_off.weapons = false;
	expect_false(trainer_logic::IsItemCategoryEnabledByClassName("weapon_railgun", weapons_off), "weapon off");
	expect_false(trainer_logic::IsItemCategoryEnabledByClassName("item_quad", weapons_off), "quad follows weapon toggle");

	trainer_logic::category_toggles_t health_off{};
	health_off.health = false;
	expect_false(trainer_logic::IsItemCategoryEnabledByClassName("item_health_combined", health_off),
		"virtual combined health follows health toggle");

	trainer_logic::category_toggles_t powerups_off{};
	powerups_off.powerups = false;
	expect_false(trainer_logic::IsItemCategoryEnabledByClassName("item_invulnerability", powerups_off),
		"remaining item_* uses powerups toggle");
}

void test_category_by_flags()
{
	trainer_logic::category_toggles_t toggles{};
	toggles.armor = false;

	expect_false(trainer_logic::IsItemCategoryEnabledForItem(trainer_logic::IF_ARMOR, "item_armor_body", toggles),
		"IF_ARMOR respects armor toggle");
	expect_true(trainer_logic::IsItemCategoryEnabledForItem(trainer_logic::IF_HEALTH, "item_health", toggles),
		"other flags unaffected");

	toggles.weapons = false;
	expect_false(trainer_logic::IsItemCategoryEnabledForItem(0, "item_quad", toggles), "quad classname without flags");
}

void test_friendly_name_from_pickup()
{
	char buf[64];

	trainer_logic::FriendlyNameFromPickup("$item_railgun", buf, sizeof(buf));
	expect_str_eq(buf, "Railgun", "pickup strips $item_ and title-cases");

	trainer_logic::FriendlyNameFromPickup("$item_super_shotgun", buf, sizeof(buf));
	expect_str_eq(buf, "Super shotgun", "pickup replaces underscores");

	trainer_logic::FriendlyNameFromPickup(nullptr, buf, sizeof(buf));
	expect_str_eq(buf, "Item", "null pickup fallback");
}

void test_timing_challenge()
{
	expect_float_eq(trainer_logic::TimingWindowSeconds(1), trainer_config::TIMING_WINDOW_EASY_SEC, "easy window");
	expect_float_eq(trainer_logic::TimingWindowSeconds(4), trainer_config::TIMING_WINDOW_PRO_SEC, "pro window");
	expect_float_eq(trainer_logic::TimingWindowSeconds(0), 0.0f, "off window");

	const auto inside = trainer_logic::EvaluateTimingChallenge(0.2f, 4);
	expect_true(inside.success, "pro success inside 1s window");
	expect_true(inside.perfect, "0.2s inside perfect threshold");

	const auto outside = trainer_logic::EvaluateTimingChallenge(2.0f, 4);
	expect_false(outside.success, "pro failure outside 1s window");
	expect_false(outside.perfect, "failure is not perfect");
}

void test_path_target_skip()
{
	expect_false(trainer_logic::ShouldSkipUniqueTypeForPick(1, "item_health", "item_health"),
		"single unique type never skipped");
	expect_true(trainer_logic::ShouldSkipUniqueTypeForPick(2, "item_armor_body", "item_armor_body"),
		"repeat type skipped with multiple types");
	expect_false(trainer_logic::ShouldSkipUniqueTypeForPick(2, "item_health", "item_armor_body"),
		"different type not skipped");
	expect_false(trainer_logic::ShouldSkipUniqueTypeForPick(2, "item_health", nullptr),
		"no previous type never skipped");
}

void test_path_item_inclusion()
{
	expect_true(trainer_logic::IsPathTrainingItemIncluded("item_health_small", true, true),
		"stimpack included when enabled");
	expect_false(trainer_logic::IsPathTrainingItemIncluded("item_health_small", false, true),
		"stimpack excluded when disabled");
	expect_true(trainer_logic::IsPathTrainingItemIncluded("item_armor_shard", true, true),
		"shard included when enabled");
	expect_false(trainer_logic::IsPathTrainingItemIncluded("item_armor_shard", true, false),
		"shard excluded when disabled");
	expect_true(trainer_logic::IsPathTrainingItemIncluded("item_armor_body", false, false),
		"other items unaffected by path toggles");
	expect_true(trainer_logic::IsPathTrainingItemIncluded("item_health", false, false),
		"med health unaffected by stimpack toggle");
}

void expect_float_near(float actual, float expected, float tolerance, const char *message)
{
	const float diff = actual > expected ? actual - expected : expected - actual;
	if (diff > tolerance)
	{
		std::printf("FAIL: %s (expected %g +/- %g, got %g)\n", message, expected, tolerance, actual);
		++g_failures;
	}
}

void test_estimate_travel_seconds()
{
	// 2000 units at 400ups with a 1.35 inefficiency factor is a 6.75s route.
	// Distances here must stay well clear of GHOST_MIN_TRAVEL_SEC or the clamp, not the
	// scaling, is what is being measured.
	expect_float_near(trainer_logic::EstimateTravelSeconds(2000.0f, 400.0f, 1.35f), 6.75f, 0.001f,
		"travel time scales distance by inefficiency");
	expect_true(trainer_logic::EstimateTravelSeconds(1600.0f, 400.0f, 1.35f) >
		trainer_logic::EstimateTravelSeconds(800.0f, 400.0f, 1.35f),
		"travel time is monotonic in distance");

	// Clamps: a zero-distance hop still costs the floor, and bad inputs never
	// produce a zero/negative/NaN travel time that would make routing degenerate.
	expect_float_near(trainer_logic::EstimateTravelSeconds(0.0f, 400.0f, 1.35f),
		trainer_config::GHOST_MIN_TRAVEL_SEC, 0.001f, "zero distance clamps to floor");
	expect_float_near(trainer_logic::EstimateTravelSeconds(-100.0f, 400.0f, 1.35f),
		trainer_config::GHOST_MIN_TRAVEL_SEC, 0.001f, "negative distance clamps to floor");
	expect_float_near(trainer_logic::EstimateTravelSeconds(400.0f, 0.0f, 1.35f),
		trainer_config::GHOST_MIN_TRAVEL_SEC, 0.001f, "zero speed clamps to floor");
	expect_float_near(trainer_logic::EstimateTravelSeconds(2000.0f, 400.0f, 0.5f), 5.0f, 0.001f,
		"inefficiency below 1 is clamped to 1");

	// The floor exists so two nearby items can never read as a single simultaneous pickup.
	expect_float_near(trainer_logic::EstimateTravelSeconds(100.0f, 400.0f, 1.0f),
		trainer_config::GHOST_MIN_TRAVEL_SEC, 0.001f, "a very short hop still costs the floor");
	expect_true(trainer_config::GHOST_MIN_TRAVEL_SEC >= 1.0f,
		"the travel floor is long enough to separate consecutive pickups");
}

void test_ghost_item_value()
{
	expect_true(trainer_logic::GhostItemValue("item_armor_body") >
		trainer_logic::GhostItemValue("item_armor_combat"),
		"red armor outranks yellow");
	expect_true(trainer_logic::GhostItemValue("item_armor_combat") >
		trainer_logic::GhostItemValue("item_armor_jacket"),
		"yellow armor outranks green");
	expect_true(trainer_logic::GhostItemValue("item_health_mega") >
		trainer_logic::GhostItemValue("weapon_railgun"),
		"megahealth outranks the trifecta weapons");

	// Value 0 is what keeps non-majors out of the ghost's cycle entirely.
	expect_true(trainer_logic::GhostItemValue("item_health_small") == 0, "stimpack is not a ghost target");
	expect_true(trainer_logic::GhostItemValue("weapon_shotgun") == 0, "shotgun is not a ghost target");
	expect_true(trainer_logic::GhostItemValue(nullptr) == 0, "null classname is not a ghost target");
}

void test_ghost_policy()
{
	using cs = trainer_logic::control_state_t;

	const auto even = trainer_logic::GhostPolicyForControl(cs::EVEN);
	const auto in_control = trainer_logic::GhostPolicyForControl(cs::IN);
	const auto out_control = trainer_logic::GhostPolicyForControl(cs::OUT);

	// EVEN must reproduce the pre-policy constants exactly - this is a regression guard on
	// "behaviour is unchanged when neither side is ahead".
	expect_float_eq(even.wait_penalty, trainer_config::GHOST_WAIT_PENALTY, "EVEN wait penalty unchanged");
	expect_float_eq(even.value_weight, trainer_config::GHOST_VALUE_WEIGHT, "EVEN value weight unchanged");
	expect_float_eq(even.player_proximity_penalty, 0.0f, "EVEN does not avoid the player");

	// Winning: happy to camp a spawn, cares more about value, ignores the player.
	expect_true(in_control.wait_penalty < even.wait_penalty, "in control is more willing to wait");
	expect_true(in_control.value_weight > even.value_weight, "in control weights value higher");
	expect_float_eq(in_control.player_proximity_penalty, 0.0f, "in control does not avoid the player");

	// Losing: keeps moving, settles for minors, actively avoids the player.
	expect_true(out_control.wait_penalty > even.wait_penalty, "out of control avoids camping");
	expect_true(out_control.value_weight < even.value_weight, "out of control settles for minor items");
	expect_true(out_control.player_proximity_penalty > 0.0f, "out of control avoids the player");
}

void test_pick_ghost_target()
{
	const float speed = trainer_config::GHOST_SPEED_UPS;
	const float ineff = trainer_config::GHOST_PATH_INEFFICIENCY;
	const auto even = trainer_logic::GhostPolicyForControl(trainer_logic::control_state_t::EVEN);

	expect_true(trainer_logic::PickGhostTarget(nullptr, 0, speed, ineff, even) == -1, "no candidates yields -1");

	// A map with only minor items gives the ghost nowhere to go.
	trainer_logic::ghost_candidate_t minor[2];
	minor[0].item_index = 7; minor[0].distance = 100.0f; minor[0].value = 0;
	minor[1].item_index = 9; minor[1].distance = 200.0f; minor[1].value = 0;
	expect_true(trainer_logic::PickGhostTarget(minor, 2, speed, ineff, even) == -1,
		"all-zero-value candidates yield -1");

	// The headline behaviour: walk past a green armor that is up now to arrive at a
	// red armor exactly as it respawns.
	trainer_logic::ghost_candidate_t choose[2];
	choose[0].item_index = 3; choose[0].distance = 400.0f;  choose[0].respawn_in_sec = 0.0f;  choose[0].value = 2;
	choose[1].item_index = 5; choose[1].distance = 3200.0f; choose[1].respawn_in_sec = 10.0f; choose[1].value = 10;
	expect_true(trainer_logic::PickGhostTarget(choose, 2, speed, ineff, even) == 1,
		"prefers a high-value item it can just reach over a low-value one it reaches early");

	// But it should not camp: a red armor 40s out loses to a yellow that is up now.
	trainer_logic::ghost_candidate_t camp[2];
	camp[0].item_index = 3; camp[0].distance = 400.0f; camp[0].respawn_in_sec = 40.0f; camp[0].value = 10;
	camp[1].item_index = 5; camp[1].distance = 800.0f; camp[1].respawn_in_sec = 2.0f;  camp[1].value = 5;
	expect_true(trainer_logic::PickGhostTarget(camp, 2, speed, ineff, even) == 1,
		"wait penalty stops the ghost camping a far-off respawn");

	// Determinism matters: trainer.log is only comparable between runs if identical
	// map state routes identically.
	trainer_logic::ghost_candidate_t tie[2];
	tie[0].item_index = 3; tie[0].distance = 800.0f; tie[0].respawn_in_sec = 0.0f; tie[0].value = 10;
	tie[1].item_index = 5; tie[1].distance = 800.0f; tie[1].respawn_in_sec = 0.0f; tie[1].value = 10;
	expect_true(trainer_logic::PickGhostTarget(tie, 2, speed, ineff, even) == 0, "ties resolve to the lowest index");

	// The point of control-aware routing: a red armor sitting right next to the player is
	// the correct pick when the ghost is stacked, and the wrong one when it is not.
	const auto in_control = trainer_logic::GhostPolicyForControl(trainer_logic::control_state_t::IN);
	const auto out_control = trainer_logic::GhostPolicyForControl(trainer_logic::control_state_t::OUT);

	trainer_logic::ghost_candidate_t contested[2];
	// [0] high value, but the player is standing on it.
	contested[0].item_index = 3; contested[0].distance = 900.0f;
	contested[0].respawn_in_sec = 0.0f; contested[0].value = 10; contested[0].player_distance = 50.0f;
	// [1] lower value, comfortably away from the player.
	contested[1].item_index = 5; contested[1].distance = 900.0f;
	contested[1].respawn_in_sec = 0.0f; contested[1].value = 5; contested[1].player_distance = 3000.0f;

	expect_true(trainer_logic::PickGhostTarget(contested, 2, speed, ineff, in_control) == 0,
		"in control, the ghost contests the high-value item regardless of the player");
	expect_true(trainer_logic::PickGhostTarget(contested, 2, speed, ineff, out_control) == 1,
		"out of control, the ghost declines a contested major and takes the safe item");
	expect_true(trainer_logic::PickGhostTarget(contested, 2, speed, ineff, even) == 0,
		"EVEN ignores player proximity, matching pre-policy behaviour");
}

void test_route_time_profile()
{
	using rp = trainer_logic::route_point_t;
	const float run = trainer_config::GHOST_SPEED_RUN;
	const float strafe = trainer_config::GHOST_SPEED_STRAFE;

	// Degenerate inputs must not divide by zero or read out of bounds.
	float degenerate[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
	expect_float_eq(trainer_logic::BuildRouteTimeProfile(nullptr, 0, run, strafe, degenerate), 0.0f,
		"null points yields zero time");
	rp single[1] = { { 0.0f, 0.0f, 0.0f } };
	expect_float_eq(trainer_logic::BuildRouteTimeProfile(single, 1, run, strafe, degenerate), 0.0f,
		"single point yields zero time");
	expect_float_eq(degenerate[0], 0.0f, "single point still writes its cumulative entry");

	// Zero-length segments must not produce infinities.
	rp coincident[3] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
	float coincident_times[3];
	const float coincident_total =
		trainer_logic::BuildRouteTimeProfile(coincident, 3, run, strafe, coincident_times);
	expect_true(coincident_total == 0.0f, "coincident points take no time");

	// A straight run of 4 points, 1000 units total.
	rp straight[4] = { { 0, 0, 0 }, { 1000, 0, 0 }, { 2000, 0, 0 }, { 3000, 0, 0 } };
	float straight_times[4];
	const float straight_total =
		trainer_logic::BuildRouteTimeProfile(straight, 4, run, strafe, straight_times);

	// A zig-zag of identical total length must be slower - that is the whole point of
	// deriving speed from geometry rather than using a constant.
	rp zigzag[4] = { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 2000, 1000, 0 } };
	float zigzag_times[4];
	const float zigzag_total =
		trainer_logic::BuildRouteTimeProfile(zigzag, 4, run, strafe, zigzag_times);

	expect_true(zigzag_total > straight_total, "a corner-heavy route is slower than a straight one");

	// Cumulative times must be monotonic and end at the total.
	for (int i = 1; i < 4; i++)
	{
		expect_true(straight_times[i] >= straight_times[i - 1], "straight cumulative times are monotonic");
		expect_true(zigzag_times[i] >= zigzag_times[i - 1], "zigzag cumulative times are monotonic");
	}
	expect_float_near(straight_times[3], straight_total, 0.0001f, "last cumulative entry equals the total");
	expect_float_eq(straight_times[0], 0.0f, "first cumulative entry is zero");

	// Speed clamps: a strafe speed below run speed must not produce a negative range.
	float clamp_times[4];
	const float clamped = trainer_logic::BuildRouteTimeProfile(straight, 4, run, run * 0.5f, clamp_times);
	expect_true(clamped > 0.0f, "strafe speed below run speed still yields a positive time");

	// Position lookup: endpoints clamp, midpoint interpolates.
	const rp at_start = trainer_logic::RoutePositionAtTime(straight, 4, straight_times, -5.0f);
	expect_float_eq(at_start.x, 0.0f, "negative elapsed clamps to the route start");
	const rp at_end = trainer_logic::RoutePositionAtTime(straight, 4, straight_times, straight_total * 2.0f);
	expect_float_eq(at_end.x, 3000.0f, "elapsed past the end clamps to the route end");
	const rp midway = trainer_logic::RoutePositionAtTime(straight, 4, straight_times, straight_total * 0.5f);
	expect_true(midway.x > 0.0f && midway.x < 3000.0f, "midway lookup lands inside the route");
}

void test_stack_and_control_state()
{
	expect_true(trainer_logic::StackValue(100, 100) == 200, "stack is health plus armor");
	expect_true(trainer_logic::StackValue(100, 0) == 100, "stack with no armor");
	// Negative inputs would otherwise flip the control readout the wrong way.
	expect_true(trainer_logic::StackValue(-50, -50) == 0, "negative inputs clamp to zero");

	using cs = trainer_logic::control_state_t;
	const int32_t band = trainer_config::CONTROL_DEADBAND_STACK;

	expect_true(trainer_logic::ControlState(200, 100, band) == cs::IN, "clear lead is in control");
	expect_true(trainer_logic::ControlState(100, 200, band) == cs::OUT, "clear deficit is out of control");
	expect_true(trainer_logic::ControlState(100, 100, band) == cs::EVEN, "equal stacks are even");

	// Deadband edges: exactly at the band is still EVEN, one past it flips.
	expect_true(trainer_logic::ControlState(100 + band, 100, band) == cs::EVEN,
		"lead exactly equal to deadband stays even");
	expect_true(trainer_logic::ControlState(100 + band + 1, 100, band) == cs::IN,
		"one over the deadband flips to in control");
	expect_true(trainer_logic::ControlState(100 - band, 100, band) == cs::EVEN,
		"deficit exactly equal to deadband stays even");
	expect_true(trainer_logic::ControlState(100 - band - 1, 100, band) == cs::OUT,
		"one under the deadband flips to out of control");

	// A zero deadband must still be usable rather than reporting EVEN forever.
	expect_true(trainer_logic::ControlState(101, 100, 0) == cs::IN, "zero deadband still discriminates");
	expect_true(trainer_logic::ControlState(100, 100, -5) == cs::EVEN, "negative deadband clamps to zero");
}

void test_hud_item_ids()
{
	using id = trainer_logic::hud_item_id_t;

	expect_true(trainer_logic::HudItemId("item_armor_body") == id::RED_ARMOR, "RA id");
	expect_true(trainer_logic::HudItemId("item_health_mega") == id::MEGAHEALTH, "MH id");
	expect_true(trainer_logic::HudItemId("weapon_railgun") == id::RAILGUN, "RG id");
	expect_true(trainer_logic::HudItemId("weapon_shotgun") == id::OTHER, "unlisted item falls back to OTHER");
	expect_true(trainer_logic::HudItemId(nullptr) == id::NONE, "null classname is NONE");

	expect_str_eq(trainer_logic::HudItemShortName(id::RED_ARMOR), "RA", "RA short name");
	expect_str_eq(trainer_logic::HudItemShortName(id::NONE), "", "NONE renders as empty");
}

void test_timer_stat_packing()
{
	using id = trainer_logic::hud_item_id_t;

	// Round-trip across the whole id range and the tenths boundaries. A silent
	// truncation here shows up on screen as a plausible but wrong countdown.
	for (int32_t raw_id = 1; raw_id <= trainer_logic::TIMER_ITEM_ID_MAX; raw_id++)
	{
		const int32_t tenths_cases[] = { 0, 1, 199, 1234, trainer_logic::TIMER_TENTHS_MAX };
		for (int32_t tenths : tenths_cases)
		{
			const int32_t packed = trainer_logic::PackTimerStat(static_cast<id>(raw_id), tenths);

			// Must stay positive: these travel through a SIGNED int16_t stat slot.
			expect_true(packed > 0 && packed <= 32767, "packed timer fits a positive int16");

			id got_id = id::NONE;
			int32_t got_tenths = -1;
			trainer_logic::UnpackTimerStat(packed, &got_id, &got_tenths);

			expect_true(static_cast<int32_t>(got_id) == raw_id, "timer id round-trips");
			expect_true(got_tenths == tenths, "timer tenths round-trip");
		}
	}

	// Empty slot.
	expect_true(trainer_logic::PackTimerStat(id::NONE, 500) == 0, "NONE packs as empty slot");
	id empty_id = id::RED_ARMOR;
	int32_t empty_tenths = 99;
	trainer_logic::UnpackTimerStat(0, &empty_id, &empty_tenths);
	expect_true(empty_id == id::NONE && empty_tenths == 0, "zero unpacks as empty");

	// Clamping, not wrapping.
	id clamp_id = id::NONE;
	int32_t clamp_tenths = -1;
	trainer_logic::UnpackTimerStat(
		trainer_logic::PackTimerStat(id::QUAD, trainer_logic::TIMER_TENTHS_MAX + 500),
		&clamp_id, &clamp_tenths);
	expect_true(clamp_id == id::QUAD, "overlong countdown keeps its item id");
	expect_true(clamp_tenths == trainer_logic::TIMER_TENTHS_MAX, "overlong countdown clamps, does not wrap");

	trainer_logic::UnpackTimerStat(trainer_logic::PackTimerStat(id::QUAD, -50), &clamp_id, &clamp_tenths);
	expect_true(clamp_tenths == 0, "negative countdown clamps to zero");
}

void test_control_stat_packing()
{
	using cs = trainer_logic::control_state_t;
	using hl = trainer_logic::hud_level_t;

	const hl levels[] = { hl::OFF, hl::CLOCK, hl::NAMES, hl::FULL };
	const cs states[] = { cs::OUT, cs::EVEN, cs::IN };
	const int32_t stacks[] = { 0, 1, 200, trainer_logic::CONTROL_STACK_MAX };

	const bool ghost_flags[] = { false, true };

	for (hl level : levels)
	{
		for (cs state : states)
		{
			for (int32_t stack : stacks)
			{
				for (bool ghost : ghost_flags)
				{
					const int32_t packed = trainer_logic::PackControlStat(level, state, stack, ghost);
					expect_true(packed >= 0 && packed <= 32767, "packed control fits a positive int16");

					hl got_level = hl::OFF;
					cs got_state = cs::EVEN;
					int32_t got_stack = -1;
					bool got_ghost = !ghost;
					trainer_logic::UnpackControlStat(packed, &got_level, &got_state, &got_stack, &got_ghost);

					expect_true(got_level == level, "hud level round-trips");
					expect_true(got_state == state, "control state round-trips");
					expect_true(got_stack == stack, "ghost stack round-trips");
					expect_true(got_ghost == ghost, "ghost-active flag round-trips");
				}
			}
		}
	}

	// Stack is 9 bits; a bigger stack must clamp rather than bleed into the state bits.
	hl over_level = hl::OFF;
	cs over_state = cs::EVEN;
	int32_t over_stack = -1;
	bool over_ghost = false;
	trainer_logic::UnpackControlStat(
		trainer_logic::PackControlStat(hl::FULL, cs::IN, trainer_logic::CONTROL_STACK_MAX + 1000, true),
		&over_level, &over_state, &over_stack, &over_ghost);
	expect_true(over_stack == trainer_logic::CONTROL_STACK_MAX, "oversized stack clamps");
	expect_true(over_state == cs::IN, "oversized stack does not corrupt the control state");
	expect_true(over_level == hl::FULL, "oversized stack does not corrupt the hud level");
	expect_true(over_ghost, "oversized stack does not corrupt the ghost-active flag");
}

void test_version_constants()
{
	expect_str_eq(TRAINER_VERSION_STRING, "1.2.3", "version string");
	expect_true(TRAINER_VERSION_MAJOR == 1 && TRAINER_VERSION_MINOR == 2 && TRAINER_VERSION_PATCH == 3,
		"version components");
}
} // namespace

int main()
{
	test_combinable_health_packs();
	test_normalize_class_name();
	test_category_by_classname();
	test_category_by_flags();
	test_friendly_name_from_pickup();
	test_timing_challenge();
	test_path_target_skip();
	test_path_item_inclusion();
	test_estimate_travel_seconds();
	test_ghost_item_value();
	test_ghost_policy();
	test_pick_ghost_target();
	test_route_time_profile();
	test_stack_and_control_state();
	test_hud_item_ids();
	test_timer_stat_packing();
	test_control_stat_packing();
	test_version_constants();

	if (g_failures == 0)
	{
		std::printf("All trainer_logic tests passed.\n");
		return 0;
	}

	std::printf("%d test(s) failed.\n", g_failures);
	return 1;
}
