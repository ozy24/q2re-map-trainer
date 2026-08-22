// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Client-game side of the trainer HUD. Kept out of trainer.h because that header is
// included by the server-game translation units, which have no cgi/player_state_t view
// of the client-game types.

#pragma once

struct player_state_t;
struct vrect_t;

// Draws the speedometer and the Ghost Duel panel. Everything it needs arrives in
// bit-packed player stats; see trainer_hud.cpp for the packing side.
void MapTrainer_DrawHud(const player_state_t *ps, vrect_t hud_vrect, int32_t scale);
