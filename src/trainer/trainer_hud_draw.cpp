// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../cg_local.h"
#include "trainer_hud_draw.h"
#include "trainer_logic.h"

// ==================== GHOST DUEL HUD - CLIENT SIDE ====================
//
// Draws the trainer's HUD from bit-packed stats. Deliberately separate from
// trainer_hud.cpp: this half compiles against the client-game headers (cgi), that half
// against the server-game headers (gi), and mixing the two in one translation unit is
// not worth the trouble.
//
// Item names live here rather than travelling over the wire - stats are int16_t and
// cannot carry strings, and the panel only needs a small fixed table of duel callouts.

namespace
{
constexpr int32_t HUD_TIMER_SLOTS = 4;

// Panel sits above the bottom HUD, right-aligned, out of the way of the crosshair.
constexpr int32_t HUD_PANEL_RIGHT_MARGIN = 16;
constexpr int32_t HUD_PANEL_TOP = 96;
constexpr int32_t HUD_SPEEDOMETER_BOTTOM_OFFSET = 80;

constexpr rgba_t hud_color_up { 120, 220, 120, 255 };     // item is back / plenty of time
constexpr rgba_t hud_color_soon { 255, 210, 90, 255 };    // inside the window - move now
constexpr rgba_t hud_color_overdue { 255, 110, 110, 255 }; // it is up and you missed it
constexpr rgba_t hud_color_dim { 170, 170, 170, 255 };

// Below this many seconds the item is close enough that you should already be moving.
constexpr float HUD_SOON_THRESHOLD_SEC = 5.0f;

const char *ControlText(trainer_logic::control_state_t state)
{
	switch (state)
	{
	case trainer_logic::control_state_t::IN: return "IN CONTROL";
	case trainer_logic::control_state_t::OUT: return "OUT OF CONTROL";
	case trainer_logic::control_state_t::EVEN:
	default: return "EVEN";
	}
}

rgba_t ControlColor(trainer_logic::control_state_t state)
{
	switch (state)
	{
	case trainer_logic::control_state_t::IN: return hud_color_up;
	case trainer_logic::control_state_t::OUT: return hud_color_overdue;
	case trainer_logic::control_state_t::EVEN:
	default: return hud_color_soon;
	}
}
} // namespace

void MapTrainer_DrawHud(const player_state_t *ps, vrect_t hud_vrect, int32_t scale)
{
	if (!ps)
		return;

	// Speedometer, unchanged in behaviour - moved here so cg_screen.cpp keeps a single
	// tagged hook instead of an inline block.
	if (ps->stats[STAT_SPEEDOMETER] > 0)
	{
		const char *speed_text = G_Fmt("{}", ps->stats[STAT_SPEEDOMETER]).data();
		const int32_t x = ((hud_vrect.x + (hud_vrect.width / 2)) * scale);
		const int32_t y = ((hud_vrect.y + hud_vrect.height - HUD_SPEEDOMETER_BOTTOM_OFFSET) * scale);

		cgi.SCR_DrawFontString(speed_text, x, y, scale, rgba_white, true, text_align_t::CENTER);
	}

	const int32_t control_packed = ps->stats[STAT_TRAINER_CONTROL];
	if (control_packed <= 0)
		return; // Ghost Duel HUD is off or idle

	trainer_logic::hud_level_t hud_level = trainer_logic::hud_level_t::OFF;
	trainer_logic::control_state_t state = trainer_logic::control_state_t::EVEN;
	int32_t ghost_stack = 0;
	bool ghost_active = false;
	trainer_logic::UnpackControlStat(control_packed, &hud_level, &state, &ghost_stack, &ghost_active);

	if (hud_level == trainer_logic::hud_level_t::OFF)
		return;

	const int32_t line_height = static_cast<int32_t>(cgi.SCR_FontLineHeight(scale));
	const int32_t x = ((hud_vrect.x + hud_vrect.width - HUD_PANEL_RIGHT_MARGIN) * scale);
	int32_t y = ((hud_vrect.y + HUD_PANEL_TOP) * scale);

	// Match clock. This is the anchor real duel timing hangs off - "RA at 7:23" is
	// meaningless without it - so it is the last thing to disappear as the crutch is
	// dialled down.
	const int32_t clock_seconds = ps->stats[STAT_TRAINER_CLOCK];
	if (clock_seconds > 0)
	{
		cgi.SCR_DrawFontString(G_Fmt("{}:{:02}", clock_seconds / 60, clock_seconds % 60).data(),
			x, y, scale, rgba_white, true, text_align_t::RIGHT);
		y += line_height;
	}

	if (hud_level == trainer_logic::hud_level_t::CLOCK)
		return;

	// Resource state. Only meaningful when there is an opponent to be in control of;
	// during plain timing practice the panel is just the timer list.
	if (ghost_active)
	{
		cgi.SCR_DrawFontString(ControlText(state), x, y, scale, ControlColor(state), true, text_align_t::RIGHT);
		y += line_height;

		cgi.SCR_DrawFontString(G_Fmt("ghost {}", ghost_stack).data(), x, y, scale,
			hud_color_dim, true, text_align_t::RIGHT);
		y += line_height;
	}

	for (int32_t i = 0; i < HUD_TIMER_SLOTS; i++)
	{
		const int32_t packed = ps->stats[STAT_TRAINER_TIMER_1 + i];
		if (packed <= 0)
			continue;

		trainer_logic::hud_item_id_t id = trainer_logic::hud_item_id_t::NONE;
		int32_t tenths = 0;
		trainer_logic::UnpackTimerStat(packed, &id, &tenths);

		if (id == trainer_logic::hud_item_id_t::NONE)
			continue;

		const char *name = trainer_logic::HudItemShortName(id);
		const float seconds = static_cast<float>(tenths) / 10.0f;

		rgba_t color = hud_color_up;
		if (tenths == 0)
			color = hud_color_overdue;
		else if (seconds <= HUD_SOON_THRESHOLD_SEC)
			color = hud_color_soon;

		// At NAMES level the server sends zero tenths on purpose: you are told which
		// item is down and you supply the number yourself.
		const char *row = (hud_level == trainer_logic::hud_level_t::FULL && tenths > 0)
			? G_Fmt("{} {:.1f}", name, seconds).data()
			: G_Fmt("{} --", name).data();

		cgi.SCR_DrawFontString(row, x, y, scale, color, true, text_align_t::RIGHT);
		y += line_height;
	}
}
