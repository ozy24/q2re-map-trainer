// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// Archived trainer settings (CVAR_ARCHIVE). Registered in InitGame via MapTrainer_RegisterCvars().
void MapTrainer_RegisterCvars();
void MapTrainer_LoadFromCvars();
void MapTrainer_WriteCvars();
