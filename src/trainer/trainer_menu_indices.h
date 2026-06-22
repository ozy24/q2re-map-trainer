// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Row indices for trainer PMenu arrays — keep in sync with maptrainer_*_submenu[] order.

#pragma once

#include <cstdint>

namespace trainer_menu
{
enum class main_entry_t : int32_t
{
	HEADER = 0,
	BLANK1,
	PATH_TRAINER,
	TIMING_TRAINER,
	JUMP_TRAINER,
	SPAWN_TRAINER,
	BLANK2,
	SPEEDOMETER,
	BLANK3,
	BLANK4,
	BLANK5,
	CLOSE,
	BLANK6,
	FOOTER,
	VERSION
};

enum class path_entry_t : int32_t
{
	HEADER = 0,
	BLANK1,
	TOGGLE,
	WEAPONS,
	AMMO,
	HEALTH,
	ARMOR,
	POWERUPS,
	COMBINE_HEALTH,
	STIMPACKS,
	ARMOR_SHARDS,
	BLANK2,
	BACK,
	BLANK3,
	FOOTER,
	VERSION
};

enum class timing_entry_t : int32_t
{
	HEADER = 0,
	BLANK1,
	TOGGLE,
	TRACK,
	FREE_COLLECT,
	CHALLENGE,
	DEBUG_PRINTS,
	BLANK2,
	BLANK3,
	BLANK4,
	BACK,
	BLANK5,
	FOOTER,
	VERSION
};

enum class jump_entry_t : int32_t
{
	HEADER = 0,
	BLANK1,
	SAVE_POS,
	LOAD_POS,
	BHOP,
	BLANK2,
	BLANK3,
	BLANK4,
	BLANK5,
	BLANK6,
	BACK,
	BLANK7,
	FOOTER,
	VERSION
};

enum class spawn_entry_t : int32_t
{
	HEADER = 0,
	BLANK1,
	TOGGLE,
	SPAWN_ORDER,
	BEACON,
	BLANK2,
	BLANK3,
	BACK,
	BLANK4,
	FOOTER,
	VERSION
};

constexpr int32_t entry_index(main_entry_t e) { return static_cast<int32_t>(e); }
constexpr int32_t entry_index(path_entry_t e) { return static_cast<int32_t>(e); }
constexpr int32_t entry_index(timing_entry_t e) { return static_cast<int32_t>(e); }
constexpr int32_t entry_index(jump_entry_t e) { return static_cast<int32_t>(e); }
constexpr int32_t entry_index(spawn_entry_t e) { return static_cast<int32_t>(e); }
} // namespace trainer_menu
