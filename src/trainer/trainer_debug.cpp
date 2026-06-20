// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include <fstream>
#include <ctime>

// ==================== CENTRALIZED DEBUG LOGGING ====================
// All trainer debug logs go to trainer.log, controlled by trainer_debug cvar

extern cvar_t *trainer_debug;

namespace {
	std::ofstream g_trainer_log;
	bool g_log_initialized = false;
	
	void EnsureLogInitialized()
	{
		if (!g_log_initialized)
		{
			// Append to trainer.log in the process working directory (see README troubleshooting)
			g_trainer_log.open("trainer.log", std::ios::app);
			if (g_trainer_log.is_open())
			{
				time_t now = time(nullptr);
				char timestamp[64];
				strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
				g_trainer_log << "==========================================\n";
				g_trainer_log << "Trainer Debug Log Session: " << timestamp << "\n";
				g_trainer_log << "==========================================\n";
				g_trainer_log.flush();
			}
			g_log_initialized = true;
		}
	}
}

void TrainerLog(const char* category, const char* format, ...)
{
	// Check if debug logging is enabled
	if (!trainer_debug || !trainer_debug->integer)
		return;
	
	EnsureLogInitialized();
	
	if (!g_trainer_log.is_open())
		return;

	// Get timestamp
	time_t now = time(nullptr);
	char timestamp[32];
	strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localtime(&now));

	// Format message
	char buffer[512];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	// Write to log: [TIME] [CATEGORY] message
	g_trainer_log << "[" << timestamp << "] [" << category << "] " << buffer << std::endl;
	g_trainer_log.flush(); // Ensure it's written immediately
}

void TrainerLog_Separator()
{
	if (!trainer_debug || !trainer_debug->integer)
		return;
	
	EnsureLogInitialized();
	
	if (!g_trainer_log.is_open())
		return;
	
	g_trainer_log << "----------------------------------------\n";
	g_trainer_log.flush();
}

