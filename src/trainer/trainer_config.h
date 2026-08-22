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

// Ghost duel opponent
// The ghost has no body and no nav mesh: travel time is straight-line distance
// scaled by an inefficiency factor. Straight-line badly underestimates real Quake
// routes, so this is calibration to make the item cadence plausible - it is not
// an attempt at correct pathing.
constexpr float GHOST_SPEED_UPS = 400.0f;
// Residual inefficiency applied ONLY to the fallback estimate, never to a measured nav
// route. Modest because the estimate already applies its own vertical and detour factors;
// this just covers imperfect movement on top of the geometry.
constexpr float GHOST_PATH_INEFFICIENCY = 1.15f;
// Nobody crosses even a small room in half a second. This floor exists so that two items
// a few hundred units apart cannot read as a single simultaneous pickup.
constexpr float GHOST_MIN_TRAVEL_SEC = 1.5f;
// Hard floor between consecutive pickups, independent of distance. Palliative - the real
// fix for implausible cadence is measuring the true route (trainer_ghost_nav.cpp) - but it
// bounds the worst case on maps with clustered items or no nav data.
constexpr float GHOST_MIN_PICKUP_INTERVAL_SEC = 2.0f;
// Routing cost weights: waiting on a spawn is wasteful but not fatal; each point
// of item value is worth this many seconds of extra travel.
constexpr float GHOST_WAIT_PENALTY = 0.5f;
constexpr float GHOST_VALUE_WEIGHT = 3.0f;
// Out-of-control avoidance: items within this range of the player are penalised, scaled
// by how close they are. An opponent on low stack does not walk into you for an armor.
constexpr float GHOST_PLAYER_AVOID_RANGE = 1200.0f;
constexpr float GHOST_PLAYER_AVOID_WEIGHT = 0.02f; // seconds of cost per unit inside range
// Route measurement via the engine nav mesh.
constexpr int32_t GHOST_MAX_NODES = 32;       // major items tracked for the distance cache
constexpr int32_t GHOST_MAX_PATH_POINTS = 96; // polyline buffer; over-run is rejected, not truncated
constexpr float GHOST_PATH_RESAMPLE_DIST = 32.0f; // smaller = denser points = less corner-cutting

// Fallback estimate when no nav data exists. Vertical travel costs far more than its
// straight-line length (ramps, stairs, jump chains), and a blocked line of sight means
// the real route goes around.
constexpr float GHOST_VERTICAL_PENALTY = 4.0f;
constexpr float GHOST_BLOCKED_DETOUR = 1.6f;

// Speed along a measured route: corners are taken at run pace, long straights build
// toward strafe-jump speed. Derived per segment from the route's own geometry.
constexpr float GHOST_SPEED_RUN = 320.0f;
constexpr float GHOST_SPEED_STRAFE = 550.0f;

// Visible ghost body. The ladder runs the same way as the timing HUD: start able to see
// it through walls to learn its routes, then dial down to realistic glimpses.
constexpr int32_t GHOST_BODY_LEVEL_OFF = 0;
constexpr int32_t GHOST_BODY_LEVEL_LINE_OF_SIGHT = 1;
constexpr int32_t GHOST_BODY_LEVEL_SEE_THROUGH = 2;
constexpr int32_t GHOST_BODY_LEVEL_MAX = GHOST_BODY_LEVEL_SEE_THROUGH;
constexpr float GHOST_BODY_ALPHA = 0.4f; // never 0 - that means "default", not invisible

// Arrival-jitter levels: Precise / Human / Loose / Sloppy.
constexpr int32_t GHOST_SKILL_LEVEL_COUNT = 4;
// Stack difference below which neither side is meaningfully in control. Without this
// the readout flips every time a shard is picked up.
constexpr int32_t CONTROL_DEADBAND_STACK = 25;

// Spawn trainer
constexpr float SPAWN_TRAINER_BEACON_INTERVAL_SEC = 2.0f;
constexpr float SPAWN_TRAINER_BEACON_INITIAL_DELAY_SEC = 0.1f;
constexpr float SPAWN_TRAINER_RESUME_RETRY_SEC = 1.0f;

// Jump / bhop training (time-based; perfect window = one server tick via gi.frame_time_ms)
constexpr float BHOP_CHAIN_TIMEOUT_SEC = 1.0f;

// Speedometer
constexpr float SPEEDOMETER_MAX_DISPLAY = 32767.0f;

// Shared
constexpr int32_t ITEM_NAME_BUFFER_SIZE = 64;
constexpr int32_t FRIENDLY_NAME_ITEM_PREFIX_LEN = 5;
constexpr float SOUND_VOLUME_FULL = 1.0f;
} // namespace trainer_config
