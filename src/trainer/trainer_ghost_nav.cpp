// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include "trainer_config.h"
#include "trainer_logic.h"

// ==================== GHOST ROUTE MEASUREMENT ====================
//
// How far is it REALLY from one item to another?
//
// The ghost originally used straight-line distance scaled by a fixed 1.35 factor. That is
// wrong in a way no single constant can fix, because it ignores walls and verticality
// entirely: on a map with stacked floors the straight line between two items can be a
// third of the walking route, so the ghost arrived several times too early and the item
// cadence read as teleporting.
//
// The engine can answer the question properly. gi.GetPathToGoal returns a real nav-mesh
// route between two world points; summing its polyline gives a true route length, and the
// same points let the ghost's body follow the actual path around corners.
//
// Not everything has nav data, so every entry point here degrades to a heuristic that is
// at least aware of walls and height, and callers are told which one they got.

static_assert(MAX_GHOST_PATH_POINTS == trainer_config::GHOST_MAX_PATH_POINTS,
	"map_trainer_ghost_t's inline leg buffer must match the path query buffer size");

namespace
{
// Node table: the map's major items, so route distances can be cached pairwise. Item
// positions are static, so each pair is measured at most once per map.
struct ghost_node_t
{
	edict_t *ent;
	vec3_t origin;
};

ghost_node_t g_nodes[trainer_config::GHOST_MAX_NODES];
int32_t g_node_count = 0;

// -1 = not yet measured. Symmetric, but stored both ways for simpler lookup.
float g_route_dist[trainer_config::GHOST_MAX_NODES][trainer_config::GHOST_MAX_NODES];

// Latched once per map: false means GetPathToGoal reported NoNavAvailable and there is no
// point asking again.
bool g_nav_available = true;
bool g_nav_probed = false;

int32_t NodeIndexFor(const edict_t *ent)
{
	for (int32_t i = 0; i < g_node_count; i++)
	{
		if (g_nodes[i].ent == ent)
			return i;
	}
	return -1;
}
} // namespace

void MapTrainer_GhostResetNav()
{
	g_node_count = 0;
	g_nav_available = true;
	g_nav_probed = false;

	for (int32_t i = 0; i < trainer_config::GHOST_MAX_NODES; i++)
	{
		g_nodes[i].ent = nullptr;
		g_nodes[i].origin = {};
		for (int32_t j = 0; j < trainer_config::GHOST_MAX_NODES; j++)
			g_route_dist[i][j] = -1.0f;
	}
}

void MapTrainer_GhostBuildNodes()
{
	MapTrainer_GhostResetNav();

	for (uint32_t i = 1; i < globals.num_edicts && g_node_count < trainer_config::GHOST_MAX_NODES; i++)
	{
		edict_t *ent = &g_edicts[i];
		if (!ent->inuse || !ent->item || !ent->item->classname)
			continue;
		if (trainer_logic::GhostItemValue(ent->item->classname) <= 0)
			continue;

		g_nodes[g_node_count].ent = ent;
		g_nodes[g_node_count].origin = ent->s.origin;
		g_node_count++;
	}

	TrainerLog("GHOSTNAV", "node table built: %d major items", g_node_count);
}

bool MapTrainer_GhostNavAvailable()
{
	return g_nav_available;
}

// Wall- and height-aware straight-line estimate. Used when nav data is missing, and to
// score candidates whose real route has not been measured yet.
float MapTrainer_GhostEstimateDistance(const vec3_t &start, const vec3_t &goal)
{
	const vec3_t diff = goal - start;

	const float horizontal = sqrtf(diff[0] * diff[0] + diff[1] * diff[1]);
	const float vertical = fabsf(diff[2]);

	// A 128-unit climb means a ramp, stair run or jump chain - not 128 units of travel.
	float effective = horizontal + vertical * trainer_config::GHOST_VERTICAL_PENALTY;

	// One trace is a cheap, strong proxy for "straight shot" versus "go around". It does
	// not tell us how far around, but it reliably separates the two cases.
	const trace_t tr = gi.traceline(start, goal, nullptr, MASK_SOLID);
	if (tr.fraction < 1.0f)
		effective *= trainer_config::GHOST_BLOCKED_DETOUR;

	return effective;
}

bool MapTrainer_GhostQueryRoute(const vec3_t &start, const vec3_t &goal,
	vec3_t *out_points, int32_t max_points, int32_t *out_count, float *out_length)
{
	if (out_count)
		*out_count = 0;

	if (!g_nav_available)
		return false;

	if (!out_points || max_points <= 0)
		return false;

	// Request shape mirrors distance_to_poi (g_target.cpp:1599), which is already tuned
	// for arbitrary world points. The generous node search matters here because item
	// origins float above the floor rather than sitting on a nav node.
	PathRequest request;
	request.start = start;
	request.goal = goal;
	request.moveDist = trainer_config::GHOST_PATH_RESAMPLE_DIST;
	request.pathFlags = PathFlags::All;
	request.nodeSearch.ignoreNodeFlags = true;
	request.nodeSearch.minHeight = 128.0f;
	request.nodeSearch.maxHeight = 128.0f;
	request.nodeSearch.radius = 1024.0f;
	request.pathPoints.array = out_points;
	request.pathPoints.count = max_points;

	PathInfo info;
	const bool ok = gi.GetPathToGoal(request, info);

	// NoNavAvailable is permanent for this map - latch it so we stop paying for calls
	// that can never succeed. Every other failure is specific to this pair.
	if (info.returnCode == PathReturnCode::NoNavAvailable)
	{
		if (!g_nav_probed)
			TrainerLog("GHOSTNAV", "no nav data for %s; using the heuristic estimate", level.mapname);
		g_nav_available = false;
		g_nav_probed = true;
		return false;
	}

	g_nav_probed = true;

	if (!ok)
		return false;

	if (info.numPathPoints <= 0)
		return false;

	// A truncated route measures SHORT, which would silently reintroduce exactly the bug
	// this module exists to fix. Refuse it rather than under-report.
	if (info.numPathPoints > max_points)
	{
		TrainerLog("GHOSTNAV", "route truncated (%d points > %d buffer); rejecting",
			info.numPathPoints, max_points);
		return false;
	}

	// The endpoints are not included in the returned array, so both ends are added
	// explicitly (g_monster.cpp:947-948 traces them separately for the same reason).
	float length = (out_points[0] - start).length();
	for (int32_t i = 0; i < info.numPathPoints - 1; i++)
		length += (out_points[i + 1] - out_points[i]).length();
	length += (goal - out_points[info.numPathPoints - 1]).length();

	if (out_count)
		*out_count = info.numPathPoints;
	if (out_length)
		*out_length = length;

	return true;
}

// Build the current leg: measure the route and turn it into a time profile.
// Returns true when a real nav route was used; false leaves leg_count == 0 and the caller
// falls back to a straight line.
bool MapTrainer_GhostBuildLeg(const vec3_t &start, const vec3_t &goal)
{
	map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	ghost.leg_count = 0;
	ghost.leg_duration = 0.0f;

	vec3_t points[trainer_config::GHOST_MAX_PATH_POINTS];
	int32_t count = 0;
	float length = 0.0f;

	if (!MapTrainer_GhostQueryRoute(start, goal, points,
		trainer_config::GHOST_MAX_PATH_POINTS, &count, &length) || count <= 0)
	{
		return false;
	}

	// Bookend the interior points with the true endpoints so the body starts and finishes
	// exactly where the item is, rather than at the nearest nav node.
	const int32_t total = min(count + 2, MAX_GHOST_PATH_POINTS);

	ghost.leg_points[0] = start;
	int32_t written = 1;
	for (int32_t i = 0; i < count && written < total - 1; i++)
		ghost.leg_points[written++] = points[i];
	ghost.leg_points[written++] = goal;
	ghost.leg_count = written;

	// trainer_logic is engine-free, so convert to its POD point type.
	trainer_logic::route_point_t profile_points[MAX_GHOST_PATH_POINTS];
	for (int32_t i = 0; i < ghost.leg_count; i++)
	{
		profile_points[i].x = ghost.leg_points[i][0];
		profile_points[i].y = ghost.leg_points[i][1];
		profile_points[i].z = ghost.leg_points[i][2];
	}

	ghost.leg_duration = trainer_logic::BuildRouteTimeProfile(profile_points, ghost.leg_count,
		trainer_config::GHOST_SPEED_RUN, trainer_config::GHOST_SPEED_STRAFE, ghost.leg_time);

	// Cache the measured length for future scoring.
	const int32_t from_index = NodeIndexFor(ghost.position_ent);
	const int32_t to_index = NodeIndexFor(ghost.target_ent);
	if (from_index >= 0 && to_index >= 0)
	{
		g_route_dist[from_index][to_index] = length;
		g_route_dist[to_index][from_index] = length;
	}

	return true;
}

// Where the ghost is right now along its current leg. Falls back to a straight-line lerp
// between route_start and target_position when there is no measured route.
vec3_t MapTrainer_GhostLegPosition(float elapsed)
{
	const map_trainer_ghost_t &ghost = level.map_trainer.ghost;

	if (ghost.leg_count <= 0)
	{
		const float total = (ghost.arrive_time - ghost.depart_time).seconds();
		float t = total > 0.0f ? elapsed / total : 1.0f;
		t = clamp(t, 0.0f, 1.0f);
		return ghost.route_start + (ghost.target_position - ghost.route_start) * t;
	}

	trainer_logic::route_point_t profile_points[MAX_GHOST_PATH_POINTS];
	for (int32_t i = 0; i < ghost.leg_count; i++)
	{
		profile_points[i].x = ghost.leg_points[i][0];
		profile_points[i].y = ghost.leg_points[i][1];
		profile_points[i].z = ghost.leg_points[i][2];
	}

	const trainer_logic::route_point_t p = trainer_logic::RoutePositionAtTime(
		profile_points, ghost.leg_count, ghost.leg_time, elapsed);

	return { p.x, p.y, p.z };
}

// Answers "does this map have usable nav data, and how wrong was straight-line distance?"
// The ratio column is the number that matters: it is the factor by which the old
// straight-line model under-estimated every route.
void Cmd_TrainerNavCheck_f(edict_t *ent)
{
	if (!ent || !ent->client)
		return;

	MapTrainer_GhostBuildNodes();

	gi.LocClient_Print(ent, PRINT_HIGH, G_Fmt("\n===== nav check: {} =====", level.mapname).data());
	gi.LocClient_Print(ent, PRINT_HIGH, G_Fmt("Major items found: {}", g_node_count).data());

	if (g_node_count < 2)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Not enough major items to measure a route.");
		return;
	}

	// Probe once so the nav-availability verdict is known before the table is printed.
	vec3_t probe_points[trainer_config::GHOST_MAX_PATH_POINTS];
	int32_t probe_count = 0;
	float probe_length = 0.0f;
	const bool nav_ok = MapTrainer_GhostQueryRoute(g_nodes[0].origin, g_nodes[1].origin,
		probe_points, trainer_config::GHOST_MAX_PATH_POINTS, &probe_count, &probe_length);

	if (!nav_ok)
	{
		gi.LocClient_Print(ent, PRINT_HIGH,
			"NAV DATA UNAVAILABLE - the ghost is using the heuristic estimate.");
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Distances below are estimates (vertical-weighted, detour-checked), not measured routes.");
	}
	else
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Nav data OK - routes are measured, not estimated.");
	}

	gi.LocClient_Print(ent, PRINT_HIGH, "  pair            straight   route    ratio   travel");

	constexpr int32_t MAX_REPORT_LINES = 24;
	int32_t lines = 0;
	float ratio_total = 0.0f;
	int32_t ratio_count = 0;

	for (int32_t i = 0; i < g_node_count; i++)
	{
		for (int32_t j = i + 1; j < g_node_count; j++)
		{
			const float straight = (g_nodes[j].origin - g_nodes[i].origin).length();
			if (straight <= 0.0f)
				continue;

			float route = 0.0f;
			int32_t count = 0;
			if (!MapTrainer_GhostQueryRoute(g_nodes[i].origin, g_nodes[j].origin, probe_points,
				trainer_config::GHOST_MAX_PATH_POINTS, &count, &route))
			{
				route = MapTrainer_GhostEstimateDistance(g_nodes[i].origin, g_nodes[j].origin);
			}
			else
			{
				// Cache the measurement while we have it - navcheck warms the table.
				g_route_dist[i][j] = route;
				g_route_dist[j][i] = route;
			}

			const float ratio = route / straight;
			ratio_total += ratio;
			ratio_count++;

			if (lines < MAX_REPORT_LINES)
			{
				const char *a = trainer_logic::HudItemShortName(
					trainer_logic::HudItemId(g_nodes[i].ent->item->classname));
				const char *b = trainer_logic::HudItemShortName(
					trainer_logic::HudItemId(g_nodes[j].ent->item->classname));

				gi.LocClient_Print(ent, PRINT_HIGH,
					G_Fmt("  {:<4}->{:<4}   {:>8.0f} {:>8.0f}   {:>5.2f}x  {:>5.1f}s",
						a, b, straight, route, ratio,
						trainer_logic::EstimateTravelSeconds(route, trainer_config::GHOST_SPEED_UPS,
							nav_ok ? 1.0f : trainer_config::GHOST_PATH_INEFFICIENCY)).data());
				lines++;
			}
		}
	}

	if (ratio_count > 0)
	{
		gi.LocClient_Print(ent, PRINT_HIGH,
			G_Fmt("Mean route/straight ratio over {} pairs: {:.2f}x", ratio_count,
				ratio_total / static_cast<float>(ratio_count)).data());
		gi.LocClient_Print(ent, PRINT_HIGH,
			"(Phase 1 assumed a flat 1.35x. Anything well above that is how much too fast the ghost was.)");
	}

	if (lines >= MAX_REPORT_LINES && ratio_count > lines)
	{
		gi.LocClient_Print(ent, PRINT_HIGH,
			G_Fmt("({} further pairs measured but not listed.)", ratio_count - lines).data());
	}

	gi.LocClient_Print(ent, PRINT_HIGH, "==============================\n");
}

float MapTrainer_GhostRouteDistance(const edict_t *from_ent, const vec3_t &from_pos,
	const edict_t *to_ent, const vec3_t &to_pos, bool measure_now)
{
	const int32_t from_index = from_ent ? NodeIndexFor(from_ent) : -1;
	const int32_t to_index = to_ent ? NodeIndexFor(to_ent) : -1;
	const bool cacheable = (from_index >= 0 && to_index >= 0);

	if (cacheable && g_route_dist[from_index][to_index] >= 0.0f)
		return g_route_dist[from_index][to_index];

	// Scoring pass: use the cheap estimate rather than paying for a pathfind on every
	// candidate. Only the committed target is measured for real.
	if (!measure_now || !g_nav_available)
		return MapTrainer_GhostEstimateDistance(from_pos, to_pos);

	vec3_t points[trainer_config::GHOST_MAX_PATH_POINTS];
	int32_t count = 0;
	float length = 0.0f;

	if (!MapTrainer_GhostQueryRoute(from_pos, to_pos, points,
		trainer_config::GHOST_MAX_PATH_POINTS, &count, &length))
	{
		return MapTrainer_GhostEstimateDistance(from_pos, to_pos);
	}

	if (cacheable)
	{
		g_route_dist[from_index][to_index] = length;
		g_route_dist[to_index][from_index] = length;
	}

	return length;
}
