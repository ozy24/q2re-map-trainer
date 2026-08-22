// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "trainer_logic.h"

#include <cmath>
#include <cctype>
#include <cstring>

namespace trainer_logic
{
namespace
{
int str_icmp(const char *a, const char *b)
{
	if (!a)
		a = "";
	if (!b)
		b = "";

	while (*a && *b)
	{
		const int d = std::tolower(static_cast<unsigned char>(*a)) - std::tolower(static_cast<unsigned char>(*b));
		if (d != 0)
			return d;
		++a;
		++b;
	}

	return std::tolower(static_cast<unsigned char>(*a)) - std::tolower(static_cast<unsigned char>(*b));
}

void str_lcpy(char *dst, const char *src, size_t size)
{
	if (!dst || size == 0)
		return;

	if (!src)
	{
		dst[0] = '\0';
		return;
	}

	std::strncpy(dst, src, size - 1);
	dst[size - 1] = '\0';
}
} // namespace

bool IsCombinableHealthPack(const char *class_name)
{
	return str_icmp(class_name, "item_health_small") == 0 ||
		str_icmp(class_name, "item_health") == 0 ||
		str_icmp(class_name, "item_health_large") == 0;
}

const char *NormalizeClassName(const char *class_name, bool combine_health_packs)
{
	if (combine_health_packs && IsCombinableHealthPack(class_name))
		return "item_health_combined";
	return class_name;
}

const char *DisplayFriendlyName(const char *class_name, const char *original_friendly_name, bool combine_health_packs)
{
	if (combine_health_packs && IsCombinableHealthPack(class_name))
		return "Health Pack";
	return original_friendly_name;
}

bool IsItemCategoryEnabledByClassName(const char *class_name, const category_toggles_t &toggles)
{
	if (!class_name || !*class_name)
		return true;

	if (str_icmp(class_name, "item_health_combined") == 0)
		return toggles.health;

	if (std::strstr(class_name, "weapon_") == class_name ||
		str_icmp(class_name, "item_quad") == 0)
	{
		return toggles.weapons;
	}

	if (std::strstr(class_name, "ammo_") == class_name)
		return toggles.ammo;

	if (std::strstr(class_name, "item_health") == class_name ||
		str_icmp(class_name, "item_health_small") == 0 ||
		str_icmp(class_name, "item_health_large") == 0 ||
		str_icmp(class_name, "item_health_mega") == 0)
	{
		return toggles.health;
	}

	if (std::strstr(class_name, "item_armor") == class_name ||
		str_icmp(class_name, "item_jacket_armor") == 0 ||
		str_icmp(class_name, "item_combat_armor") == 0 ||
		str_icmp(class_name, "item_body_armor") == 0 ||
		str_icmp(class_name, "item_armor_jacket") == 0 ||
		str_icmp(class_name, "item_armor_combat") == 0 ||
		str_icmp(class_name, "item_armor_body") == 0 ||
		str_icmp(class_name, "item_power_screen") == 0 ||
		str_icmp(class_name, "item_power_shield") == 0)
	{
		return toggles.armor;
	}

	if (std::strstr(class_name, "item_") == class_name)
		return toggles.powerups;

	return true;
}

bool IsItemCategoryEnabledForItem(uint32_t flags, const char *class_name, const category_toggles_t &toggles)
{
	if (class_name && str_icmp(class_name, "item_quad") == 0)
		return toggles.weapons;

	if (flags & IF_WEAPON)
		return toggles.weapons;
	if (flags & IF_AMMO)
		return toggles.ammo;
	if (flags & IF_HEALTH)
		return toggles.health;
	if (flags & IF_ARMOR)
		return toggles.armor;
	if (flags & IF_POWERUP)
		return toggles.powerups;

	if (class_name)
		return IsItemCategoryEnabledByClassName(class_name, toggles);

	return true;
}

void FriendlyNameFromPickup(const char *pickup_name, char *out, size_t out_size)
{
	if (!pickup_name || !*pickup_name)
	{
		str_lcpy(out, "Item", out_size);
		return;
	}

	if (pickup_name[0] == '$')
	{
		const char *name = pickup_name + 1;
		if (std::strncmp(name, "item_", trainer_config::FRIENDLY_NAME_ITEM_PREFIX_LEN) == 0)
			name += trainer_config::FRIENDLY_NAME_ITEM_PREFIX_LEN;

		if (*name)
		{
			char buf[64];
			str_lcpy(buf, name, sizeof(buf));
			buf[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(buf[0])));
			for (size_t i = 1; buf[i]; ++i)
			{
				if (buf[i] == '_')
					buf[i] = ' ';
				else
					buf[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(buf[i])));
			}
			str_lcpy(out, buf, out_size);
			return;
		}
	}

	str_lcpy(out, pickup_name, out_size);
}

float TimingWindowSeconds(int32_t challenge_mode)
{
	switch (challenge_mode)
	{
	case 1: return trainer_config::TIMING_WINDOW_EASY_SEC;
	case 2: return trainer_config::TIMING_WINDOW_MEDIUM_SEC;
	case 3: return trainer_config::TIMING_WINDOW_HARD_SEC;
	case 4: return trainer_config::TIMING_WINDOW_PRO_SEC;
	case 0:
	default: return 0.0f;
	}
}

timing_challenge_result_t EvaluateTimingChallenge(float time_diff_sec, int32_t challenge_mode)
{
	timing_challenge_result_t result;
	const float window = TimingWindowSeconds(challenge_mode);
	if (window <= 0.0f)
		return result;

	const float abs_diff = std::fabs(time_diff_sec);
	result.success = abs_diff <= window;
	result.perfect = result.success && abs_diff <= trainer_config::TIMING_PERFECT_THRESHOLD_SEC;
	return result;
}

bool ShouldSkipUniqueTypeForPick(int32_t unique_item_count, const char *unique_class_name,
	const char *previous_normalized_class_name)
{
	if (unique_item_count <= 1 || !previous_normalized_class_name || !unique_class_name)
		return false;

	return str_icmp(unique_class_name, previous_normalized_class_name) == 0;
}

bool IsPathTrainingItemIncluded(const char *class_name, bool stimpacks_enabled, bool armor_shards_enabled)
{
	if (!class_name)
		return true;

	if (str_icmp(class_name, "item_health_small") == 0)
		return stimpacks_enabled;

	if (str_icmp(class_name, "item_armor_shard") == 0)
		return armor_shards_enabled;

	return true;
}

// ==================== GHOST DUEL OPPONENT ====================

float EstimateTravelSeconds(float distance, float speed_ups, float inefficiency)
{
	if (speed_ups <= 0.0f)
		return trainer_config::GHOST_MIN_TRAVEL_SEC;

	if (distance < 0.0f)
		distance = 0.0f;
	if (inefficiency < 1.0f)
		inefficiency = 1.0f;

	const float seconds = (distance / speed_ups) * inefficiency;
	return seconds < trainer_config::GHOST_MIN_TRAVEL_SEC ? trainer_config::GHOST_MIN_TRAVEL_SEC : seconds;
}

int32_t GhostItemValue(const char *class_name)
{
	if (!class_name)
		return 0;

	// Mirrors MapTrainer_IsMajorItem's set (trainer_timing.cpp) but ranked, because
	// the ghost has to choose between majors rather than just recognise them.
	if (str_icmp(class_name, "item_armor_body") == 0)			// Red Armor
		return 10;
	if (str_icmp(class_name, "item_quad") == 0)					// Quad Damage
		return 10;
	if (str_icmp(class_name, "item_health_mega") == 0)			// Megahealth
		return 9;
	if (str_icmp(class_name, "item_armor_combat") == 0)			// Yellow Armor
		return 5;
	if (str_icmp(class_name, "weapon_railgun") == 0)
		return 4;
	if (str_icmp(class_name, "weapon_rocketlauncher") == 0)
		return 4;
	if (str_icmp(class_name, "weapon_chaingun") == 0)
		return 4;
	if (str_icmp(class_name, "item_armor_jacket") == 0)			// Green Armor
		return 2;

	return 0;
}

ghost_policy_t GhostPolicyForControl(control_state_t state)
{
	ghost_policy_t policy;

	switch (state)
	{
	case control_state_t::IN:
		// Winning: hold the major cycle. Willing to stand on a spawn and wait, weights
		// value heavily, and does not care where the player is - it has the stack to
		// contest anything.
		policy.wait_penalty = trainer_config::GHOST_WAIT_PENALTY * 0.25f;
		policy.value_weight = trainer_config::GHOST_VALUE_WEIGHT * 1.5f;
		policy.player_proximity_penalty = 0.0f;
		break;

	case control_state_t::OUT:
		// Losing: survive and look for one steal. Avoids the player, settles for minor
		// items, and keeps moving rather than camping contested ground.
		policy.wait_penalty = trainer_config::GHOST_WAIT_PENALTY * 2.5f;
		policy.value_weight = trainer_config::GHOST_VALUE_WEIGHT * 0.5f;
		policy.player_proximity_penalty = trainer_config::GHOST_PLAYER_AVOID_WEIGHT;
		break;

	case control_state_t::EVEN:
	default:
		// Exactly the pre-policy behaviour.
		policy.wait_penalty = trainer_config::GHOST_WAIT_PENALTY;
		policy.value_weight = trainer_config::GHOST_VALUE_WEIGHT;
		policy.player_proximity_penalty = 0.0f;
		break;
	}

	return policy;
}

int32_t PickGhostTarget(const ghost_candidate_t *candidates, int32_t count,
	float speed_ups, float inefficiency, const ghost_policy_t &policy)
{
	if (!candidates || count <= 0)
		return -1;

	int32_t best = -1;
	float best_cost = 0.0f;

	for (int32_t i = 0; i < count; i++)
	{
		if (candidates[i].value <= 0)
			continue;

		const float travel = EstimateTravelSeconds(candidates[i].distance, speed_ups, inefficiency);
		const float respawn_in = candidates[i].respawn_in_sec > 0.0f ? candidates[i].respawn_in_sec : 0.0f;
		// The ghost cannot take the item before it exists, so acquisition is the later
		// of "I arrive" and "it respawns"; the gap between them is dead time.
		const float acquire_at = travel > respawn_in ? travel : respawn_in;
		const float wait = acquire_at - travel;

		// Closeness to the player, not distance: an item right next to them is the one an
		// out-of-control opponent most wants to avoid. Zero weight ignores this entirely.
		float proximity_cost = 0.0f;
		if (policy.player_proximity_penalty > 0.0f)
		{
			const float closeness = trainer_config::GHOST_PLAYER_AVOID_RANGE - candidates[i].player_distance;
			if (closeness > 0.0f)
				proximity_cost = policy.player_proximity_penalty * closeness;
		}

		const float cost = acquire_at
			+ policy.wait_penalty * wait
			+ proximity_cost
			- policy.value_weight * static_cast<float>(candidates[i].value);

		// Strict < keeps the lowest index on ties, so a given map state always routes
		// the same way and trainer.log stays comparable between runs.
		if (best < 0 || cost < best_cost)
		{
			best = i;
			best_cost = cost;
		}
	}

	return best;
}

// ==================== ROUTE TIMING ====================

namespace
{
float PointDistance(const route_point_t &a, const route_point_t &b)
{
	const float dx = b.x - a.x;
	const float dy = b.y - a.y;
	const float dz = b.z - a.z;
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Cosine of the turn angle at point `mid`. 1 = dead straight, -1 = doubling back.
// Returns 1 for degenerate (zero-length) segments so they don't force a speed penalty.
float TurnCosine(const route_point_t &prev, const route_point_t &mid, const route_point_t &next)
{
	const float ax = mid.x - prev.x, ay = mid.y - prev.y, az = mid.z - prev.z;
	const float bx = next.x - mid.x, by = next.y - mid.y, bz = next.z - mid.z;

	const float la = std::sqrt(ax * ax + ay * ay + az * az);
	const float lb = std::sqrt(bx * bx + by * by + bz * bz);
	if (la <= 0.0f || lb <= 0.0f)
		return 1.0f;

	const float dot = (ax * bx + ay * by + az * bz) / (la * lb);
	return dot < -1.0f ? -1.0f : (dot > 1.0f ? 1.0f : dot);
}
} // namespace

float BuildRouteTimeProfile(const route_point_t *points, int32_t count,
	float run_speed, float strafe_speed, float *out_cumulative_time)
{
	if (!points || count <= 0 || !out_cumulative_time)
		return 0.0f;

	if (run_speed <= 0.0f)
		run_speed = 1.0f;
	if (strafe_speed < run_speed)
		strafe_speed = run_speed;

	out_cumulative_time[0] = 0.0f;
	if (count == 1)
		return 0.0f;

	float total = 0.0f;

	for (int32_t i = 0; i < count - 1; i++)
	{
		const float length = PointDistance(points[i], points[i + 1]);

		// Straightness of the turn entering this segment. The first segment has no
		// preceding one, so treat it as a standing start: run speed.
		float straightness = 0.0f;
		if (i > 0)
		{
			const float cosine = TurnCosine(points[i - 1], points[i], points[i + 1]);
			// Map [0,1] of the cosine onto [0,1]; anything at or past 90 degrees is a
			// full stop-and-turn and gets no speed bonus at all.
			straightness = cosine > 0.0f ? cosine : 0.0f;
		}

		const float speed = run_speed + (strafe_speed - run_speed) * straightness;
		total += length / speed;
		out_cumulative_time[i + 1] = total;
	}

	return total;
}

route_point_t RoutePositionAtTime(const route_point_t *points, int32_t count,
	const float *cumulative_time, float elapsed)
{
	if (!points || count <= 0 || !cumulative_time)
		return {};

	if (count == 1 || elapsed <= 0.0f)
		return points[0];

	const float total = cumulative_time[count - 1];
	if (elapsed >= total)
		return points[count - 1];

	// Find the segment containing `elapsed`. Routes are short (< 100 points) so a linear
	// scan is cheaper than the branch cost of a binary search.
	int32_t seg = 0;
	while (seg < count - 2 && cumulative_time[seg + 1] < elapsed)
		seg++;

	const float span = cumulative_time[seg + 1] - cumulative_time[seg];
	const float t = span > 0.0f ? (elapsed - cumulative_time[seg]) / span : 0.0f;

	route_point_t out;
	out.x = points[seg].x + (points[seg + 1].x - points[seg].x) * t;
	out.y = points[seg].y + (points[seg + 1].y - points[seg].y) * t;
	out.z = points[seg].z + (points[seg + 1].z - points[seg].z) * t;
	return out;
}

// ==================== CONTROL STATE ====================

int32_t StackValue(int32_t health, int32_t armor)
{
	if (health < 0)
		health = 0;
	if (armor < 0)
		armor = 0;

	return health + armor;
}

control_state_t ControlState(int32_t player_stack, int32_t ghost_stack, int32_t deadband)
{
	if (deadband < 0)
		deadband = 0;

	const int32_t diff = player_stack - ghost_stack;

	if (diff > deadband)
		return control_state_t::IN;
	if (diff < -deadband)
		return control_state_t::OUT;

	return control_state_t::EVEN;
}

// ==================== HUD STAT PACKING ====================

hud_item_id_t HudItemId(const char *class_name)
{
	if (!class_name)
		return hud_item_id_t::NONE;

	if (str_icmp(class_name, "item_armor_body") == 0)
		return hud_item_id_t::RED_ARMOR;
	if (str_icmp(class_name, "item_armor_combat") == 0)
		return hud_item_id_t::YELLOW_ARMOR;
	if (str_icmp(class_name, "item_armor_jacket") == 0)
		return hud_item_id_t::GREEN_ARMOR;
	if (str_icmp(class_name, "item_health_mega") == 0)
		return hud_item_id_t::MEGAHEALTH;
	if (str_icmp(class_name, "item_quad") == 0)
		return hud_item_id_t::QUAD;
	if (str_icmp(class_name, "weapon_railgun") == 0)
		return hud_item_id_t::RAILGUN;
	if (str_icmp(class_name, "weapon_rocketlauncher") == 0)
		return hud_item_id_t::ROCKET_LAUNCHER;
	if (str_icmp(class_name, "weapon_chaingun") == 0)
		return hud_item_id_t::CHAINGUN;

	return hud_item_id_t::OTHER;
}

const char *HudItemShortName(hud_item_id_t id)
{
	switch (id)
	{
	case hud_item_id_t::RED_ARMOR: return "RA";
	case hud_item_id_t::YELLOW_ARMOR: return "YA";
	case hud_item_id_t::GREEN_ARMOR: return "GA";
	case hud_item_id_t::MEGAHEALTH: return "MH";
	case hud_item_id_t::QUAD: return "QUAD";
	case hud_item_id_t::RAILGUN: return "RG";
	case hud_item_id_t::ROCKET_LAUNCHER: return "RL";
	case hud_item_id_t::CHAINGUN: return "CG";
	case hud_item_id_t::OTHER: return "ITEM";
	case hud_item_id_t::NONE:
	default: return "";
	}
}

int32_t PackTimerStat(hud_item_id_t item_id, int32_t tenths)
{
	const int32_t id = static_cast<int32_t>(item_id);
	if (id <= 0 || id > TIMER_ITEM_ID_MAX)
		return 0; // empty slot

	// Clamp rather than wrap: a wrapped countdown would read as a plausible but wrong
	// number on screen, which is worse than pinning at the maximum.
	if (tenths < 0)
		tenths = 0;
	if (tenths > TIMER_TENTHS_MAX)
		tenths = TIMER_TENTHS_MAX;

	return (id << TIMER_TENTHS_BITS) | tenths;
}

void UnpackTimerStat(int32_t packed, hud_item_id_t *out_item_id, int32_t *out_tenths)
{
	if (packed <= 0)
	{
		if (out_item_id)
			*out_item_id = hud_item_id_t::NONE;
		if (out_tenths)
			*out_tenths = 0;
		return;
	}

	if (out_item_id)
		*out_item_id = static_cast<hud_item_id_t>((packed >> TIMER_TENTHS_BITS) & TIMER_ITEM_ID_MAX);
	if (out_tenths)
		*out_tenths = packed & TIMER_TENTHS_MAX;
}

int32_t PackControlStat(hud_level_t hud_level, control_state_t state, int32_t ghost_stack,
	bool ghost_active)
{
	if (ghost_stack < 0)
		ghost_stack = 0;
	if (ghost_stack > CONTROL_STACK_MAX)
		ghost_stack = CONTROL_STACK_MAX;

	int32_t level_bits = static_cast<int32_t>(hud_level);
	if (level_bits < 0 || level_bits > 3)
		level_bits = 0;

	// Shift OUT/EVEN/IN (-1/0/1) up into an unsigned 0/1/2 so it survives the packing.
	const int32_t state_bits = static_cast<int32_t>(state) + 1;

	return (ghost_active ? (1 << 13) : 0) | (level_bits << 11) | (state_bits << 9) | ghost_stack;
}

void UnpackControlStat(int32_t packed, hud_level_t *out_hud_level, control_state_t *out_state,
	int32_t *out_ghost_stack, bool *out_ghost_active)
{
	if (packed < 0)
		packed = 0;

	if (out_hud_level)
		*out_hud_level = static_cast<hud_level_t>((packed >> 11) & 0x3);
	if (out_state)
		*out_state = static_cast<control_state_t>(((packed >> 9) & 0x3) - 1);
	if (out_ghost_stack)
		*out_ghost_stack = packed & CONTROL_STACK_MAX;
	if (out_ghost_active)
		*out_ghost_active = ((packed >> 13) & 0x1) != 0;
}
} // namespace trainer_logic
