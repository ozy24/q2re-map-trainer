// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Engine-free trainer logic — host-testable, shared by the game DLL and trainer_tests.

#pragma once

#include "trainer_config.h"

#include <cstddef>
#include <cstdint>

namespace trainer_logic
{
// Mirror gitem_t::flags bits used for category filtering (g_local.h item_flags_t).
constexpr uint32_t IF_WEAPON = 1u << 0;
constexpr uint32_t IF_AMMO = 1u << 1;
constexpr uint32_t IF_ARMOR = 1u << 2;
constexpr uint32_t IF_POWERUP = 1u << 5;
constexpr uint32_t IF_HEALTH = 1u << 7;

struct category_toggles_t
{
	bool weapons = true;
	bool ammo = true;
	bool health = true;
	bool armor = true;
	bool powerups = true;
};

bool IsCombinableHealthPack(const char *class_name);
const char *NormalizeClassName(const char *class_name, bool combine_health_packs);
const char *DisplayFriendlyName(const char *class_name, const char *original_friendly_name, bool combine_health_packs);
bool IsItemCategoryEnabledByClassName(const char *class_name, const category_toggles_t &toggles);
bool IsItemCategoryEnabledForItem(uint32_t flags, const char *class_name, const category_toggles_t &toggles);
void FriendlyNameFromPickup(const char *pickup_name, char *out, size_t out_size);

float TimingWindowSeconds(int32_t challenge_mode);
struct timing_challenge_result_t
{
	bool success = false;
	bool perfect = false;
};
timing_challenge_result_t EvaluateTimingChallenge(float time_diff_sec, int32_t challenge_mode);

bool ShouldSkipUniqueTypeForPick(int32_t unique_item_count, const char *unique_class_name,
	const char *previous_normalized_class_name);

bool IsPathTrainingItemIncluded(const char *class_name, bool stimpacks_enabled, bool armor_shards_enabled);

// ==================== CONTROL STATE ====================
//
// Declared before the ghost section because the ghost's routing policy is keyed on it.

enum class control_state_t : int32_t
{
	OUT = -1, // opponent is stacked; survive, chip, steal the next cycle
	EVEN = 0,
	IN = 1 // you are stacked; extend the lead, deny the next major
};

// The "stack" a duel player means when they say it: effective hit points.
int32_t StackValue(int32_t health, int32_t armor);

// Deadband keeps the readout from flickering when the two stacks are near-equal.
// ControlState(first, second) reports how the FIRST party is doing.
control_state_t ControlState(int32_t player_stack, int32_t ghost_stack, int32_t deadband);

// ==================== GHOST DUEL OPPONENT ====================

// Straight-line distance converted to a plausible route time, clamped to a floor.
float EstimateTravelSeconds(float distance, float speed_ups, float inefficiency);

// Duel priority weight used by the ghost's routing. 0 means "not worth routing to",
// which is what excludes non-major items from the ghost's cycle.
int32_t GhostItemValue(const char *class_name);

struct ghost_candidate_t
{
	int32_t item_index = -1;     // caller's own index; echoed back, never interpreted here
	float distance = 0.0f;       // route units from the ghost's current position
	float respawn_in_sec = 0.0f; // seconds until available; <= 0 means available now
	int32_t value = 0;           // GhostItemValue for this item
	float player_distance = 0.0f; // route units from the PLAYER to this item
};

// How the ghost weighs its options. A stacked opponent and a desperate one do not want
// the same things, and a ghost that always runs the optimal cycle never behaves like a
// player under pressure.
struct ghost_policy_t
{
	float wait_penalty = 0.0f;             // low = willing to camp a spawn and wait
	float value_weight = 0.0f;             // how much item value outweighs travel time
	float player_proximity_penalty = 0.0f; // per-unit cost of being near the player
};

// EVEN reproduces the pre-policy constants exactly, so behaviour is unchanged whenever
// neither side is ahead.
ghost_policy_t GhostPolicyForControl(control_state_t state);

// Returns the index INTO candidates[] of the best target, or -1 if none is viable.
// Ties resolve to the lowest index so routing is deterministic.
int32_t PickGhostTarget(const ghost_candidate_t *candidates, int32_t count,
	float speed_ups, float inefficiency, const ghost_policy_t &policy);

// ==================== ROUTE TIMING ====================
//
// A duel player does not cover ground at a constant rate: they build strafe-jump speed on
// long straight runs and bleed it in corners. Deriving speed from the route's own geometry
// makes the item cadence match what an opponent actually achieves.

// Plain POD so this stays engine-free and host-testable; callers convert from vec3_t.
struct route_point_t
{
	float x = 0.0f, y = 0.0f, z = 0.0f;
};

// Walks the polyline assigning each segment a speed between run_speed and strafe_speed
// based on how sharply the route turns at that point, and writes the cumulative arrival
// time for each point into out_cumulative_time (which must hold `count` floats).
// Returns the total traversal time in seconds.
//
// One function produces both the committed travel time AND the time->position mapping the
// body uses, so the ghost's arrival and its on-screen position cannot drift apart.
float BuildRouteTimeProfile(const route_point_t *points, int32_t count,
	float run_speed, float strafe_speed, float *out_cumulative_time);

// Position along the polyline at a given elapsed time, using the profile above.
// Clamps to the endpoints outside [0, total].
route_point_t RoutePositionAtTime(const route_point_t *points, int32_t count,
	const float *cumulative_time, float elapsed);

// ==================== HUD STAT PACKING ====================
//
// Player stats are int16_t, so every HUD payload is bit-packed into one slot. Bit 15 is
// always left clear: these travel as SIGNED int16_t, and letting the top bit set would
// sign-extend into a negative on the way out.

// How much of the timing panel is shown. The point is to start with the crutch and
// dial it down as the rhythm internalises.
enum class hud_level_t : int32_t
{
	OFF = 0,
	CLOCK = 1, // match clock only - you do the arithmetic
	NAMES = 2, // which items are down, but not how long
	FULL = 3   // live countdowns
};

// Compact duel callouts rather than full item names - the panel has no room, and these
// are what the items are actually called.
enum class hud_item_id_t : int32_t
{
	NONE = 0,
	RED_ARMOR,
	YELLOW_ARMOR,
	GREEN_ARMOR,
	MEGAHEALTH,
	QUAD,
	RAILGUN,
	ROCKET_LAUNCHER,
	CHAINGUN,
	OTHER
};

hud_item_id_t HudItemId(const char *class_name);
const char *HudItemShortName(hud_item_id_t id);

// Timer slot: item id in bits 11-14, tenths of a second in bits 0-10.
// A packed value of 0 means "slot empty", so item ids start at 1.
constexpr int32_t TIMER_TENTHS_BITS = 11;
constexpr int32_t TIMER_TENTHS_MAX = (1 << TIMER_TENTHS_BITS) - 1; // 2047 tenths = 204.7s
constexpr int32_t TIMER_ITEM_ID_MAX = 15;

int32_t PackTimerStat(hud_item_id_t item_id, int32_t tenths);
void UnpackTimerStat(int32_t packed, hud_item_id_t *out_item_id, int32_t *out_tenths);

// Control slot: ghost stack in bits 0-8, control state in bits 9-10, hud level in 11-12,
// ghost-active flag in bit 13. The flag matters because the timer panel is useful during
// plain timing practice, but the control readout is meaningless without an opponent.
constexpr int32_t CONTROL_STACK_MAX = 511;

int32_t PackControlStat(hud_level_t hud_level, control_state_t state, int32_t ghost_stack,
	bool ghost_active);
void UnpackControlStat(int32_t packed, hud_level_t *out_hud_level, control_state_t *out_state,
	int32_t *out_ghost_stack, bool *out_ghost_active);
} // namespace trainer_logic
