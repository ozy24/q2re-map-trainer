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
} // namespace trainer_logic
