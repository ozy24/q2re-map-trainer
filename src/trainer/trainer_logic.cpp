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
} // namespace trainer_logic
