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

void test_version_constants()
{
	expect_str_eq(TRAINER_VERSION_STRING, "1.2.2", "version string");
	expect_true(TRAINER_VERSION_MAJOR == 1 && TRAINER_VERSION_MINOR == 2 && TRAINER_VERSION_PATCH == 2,
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
	test_version_constants();

	if (g_failures == 0)
	{
		std::printf("All trainer_logic tests passed.\n");
		return 0;
	}

	std::printf("%d test(s) failed.\n", g_failures);
	return 1;
}
