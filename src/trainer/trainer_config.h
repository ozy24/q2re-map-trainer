// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Named trainer tuning constants (see docs/IMPROVEMENTS.md #9).

#pragma once

#include <cstdint>

namespace trainer_config
{
// Item path training
constexpr float PATH_ITEM_POSITION_EPSILON = 32.0f;

// Item timing training
constexpr float TIMING_ITEM_PICKUP_RADIUS = 64.0f;
constexpr float TIMING_ITEM_POSITION_EPSILON = 16.0f;
constexpr float TIMING_PERFECT_THRESHOLD_SEC = 0.5f;
constexpr int32_t TIMING_CHALLENGE_MODE_COUNT = 5;

constexpr float TIMING_WINDOW_EASY_SEC = 8.0f;
constexpr float TIMING_WINDOW_MEDIUM_SEC = 5.0f;
constexpr float TIMING_WINDOW_HARD_SEC = 3.0f;
constexpr float TIMING_WINDOW_PRO_SEC = 1.0f;

// Spawn trainer
constexpr float SPAWN_TRAINER_BEACON_INTERVAL_SEC = 2.0f;
constexpr float SPAWN_TRAINER_BEACON_INITIAL_DELAY_SEC = 0.1f;
constexpr float SPAWN_TRAINER_RESUME_RETRY_SEC = 1.0f;

// Jump / bhop training (frame-based until Phase 3 time conversion)
constexpr int32_t BHOP_CHAIN_TIMEOUT_FRAMES = 30;
constexpr int32_t BHOP_PERFECT_MAX_FRAMES = 1;
constexpr int32_t BHOP_DEBUG_LOG_INTERVAL_FRAMES = 10;

// Speedometer
constexpr float SPEEDOMETER_MAX_DISPLAY = 32767.0f;

// Shared
constexpr int32_t ITEM_NAME_BUFFER_SIZE = 64;
constexpr int32_t FRIENDLY_NAME_ITEM_PREFIX_LEN = 5;
constexpr float SOUND_VOLUME_FULL = 1.0f;
} // namespace trainer_config
