// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "trainer.h"
#include "trainer_config.h"
#include "trainer_logic.h"

#include <ctime>
#include <fstream>
#include <string>

// ==================== ITEM RUN RECORDS ====================
//
// Every completed Item Run is appended to RECORDS_FILE in the game's working directory
// (on Steam that is the install's rerelease/ folder, next to trainer.log). The engine
// gives the game DLL no filesystem API, so this is plain fstream, like trainer_debug.cpp.
//
// Only ranked runs (the full item set, see trainer_logic::IsRunRanked) make the top
// table; every run is still written to the file with its ranked flag. The current map's
// table is cached here and reloaded at every map start. It lives in this file rather than
// in map_trainer_t because trainer.h deliberately does not include trainer_logic.h.

namespace
{
constexpr const char *RECORDS_FILE = "maptrainer_runs.txt";
constexpr const char *RECORDS_HEADER =
	"# Q2RE Map Trainer item runs: date<TAB>map<TAB>items<TAB>time_ms<TAB>ranked (1 = every item on)";

trainer_logic::run_record_t g_records[trainer_config::RUN_RECORDS_MAX];
int32_t g_record_count = 0;

// Ranked runs on `map`, fastest first. A missing file is simply an empty table, and
// malformed lines are skipped, so a hand-edited file can never stop the game.
void LoadTable(const char *map, trainer_logic::run_record_t *table, int32_t *count)
{
	*count = 0;
	std::ifstream file(RECORDS_FILE);
	if (!file)
		return;

	std::string line;
	char line_map[64];
	trainer_logic::run_record_t record;
	bool ranked = false;
	while (std::getline(file, line))
	{
		if (!trainer_logic::ParseRunRecordLine(line.c_str(), line_map, sizeof(line_map), &record, &ranked))
			continue;
		if (!ranked || Q_strcasecmp(line_map, map) != 0)
			continue;
		trainer_logic::InsertTopRun(table, count, trainer_config::RUN_RECORDS_MAX, record);
	}
}

void FormatRow(const trainer_logic::run_record_t &record, int32_t rank, bool full_date, char *out, size_t out_size)
{
	char run_time[32];
	trainer_logic::FormatRunTime(record.run_ms, run_time, sizeof(run_time));
	// Menu rows are 63 chars at most, so the menu shows the day and the console the minute.
	char date[sizeof(record.date)];
	Q_strlcpy(date, record.date, full_date ? 17 : 11);
	Q_strlcpy(out, G_Fmt("{:2}. {:>8}  {:3} items  {}", rank, run_time, record.items, date).data(), out_size);
}
} // namespace

void MapTrainer_RecordsLoad()
{
	LoadTable(level.mapname, g_records, &g_record_count);
	TrainerLog("RUN", "loaded %d ranked run(s) for %s from %s", g_record_count, level.mapname, RECORDS_FILE);
}

int32_t MapTrainer_RecordsAddRun(int64_t run_ms, int32_t items, bool ranked)
{
	trainer_logic::run_record_t record;
	record.run_ms = run_ms;
	record.items = items;
	const time_t now = time(nullptr);
	if (const tm *local = localtime(&now))
		strftime(record.date, sizeof(record.date), "%Y-%m-%d %H:%M:%S", local);

	bool file_exists = false;
	{
		std::ifstream probe(RECORDS_FILE);
		file_exists = probe.good();
	}

	std::ofstream file(RECORDS_FILE, std::ios::app);
	if (file)
	{
		char line[160];
		trainer_logic::FormatRunRecordLine(record.date, level.mapname, items, run_ms, ranked, line, sizeof(line));
		if (!file_exists)
			file << RECORDS_HEADER << '\n';
		file << line << '\n';
		file.flush();
	}
	if (!file)
		TrainerLog("RUN", "could not write %s; the run is shown but not saved", RECORDS_FILE);

	if (!ranked)
		return 0;
	return trainer_logic::InsertTopRun(g_records, &g_record_count, trainer_config::RUN_RECORDS_MAX, record);
}

int32_t MapTrainer_RecordsCount()
{
	return g_record_count;
}

int64_t MapTrainer_RecordsBestMs()
{
	return g_record_count > 0 ? g_records[0].run_ms : 0;
}

void MapTrainer_RecordsFormatMenuRow(int32_t index, char *out, size_t out_size)
{
	if (index < 0 || index >= g_record_count)
	{
		if (out_size > 0)
			out[0] = '\0';
		return;
	}
	FormatRow(g_records[index], index + 1, false, out, out_size);
}

void MapTrainer_RecordsPrint(edict_t *player, const char *map)
{
	if (!player || !player->client)
		return;

	const bool current_map = !map || !*map || Q_strcasecmp(map, level.mapname) == 0;
	const char *shown_map = current_map ? level.mapname : map;

	trainer_logic::run_record_t other[trainer_config::RUN_RECORDS_MAX];
	int32_t other_count = 0;
	const trainer_logic::run_record_t *table = g_records;
	int32_t count = g_record_count;
	if (!current_map)
	{
		LoadTable(shown_map, other, &other_count);
		table = other;
		count = other_count;
	}

	gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("\n===== Item Run records: {} =====", shown_map).data());
	if (count == 0)
	{
		gi.LocClient_Print(player, PRINT_HIGH, "No ranked runs yet.");
	}
	else
	{
		char row[96];
		for (int32_t i = 0; i < count; i++)
		{
			FormatRow(table[i], i + 1, true, row, sizeof(row));
			gi.LocClient_Print(player, PRINT_HIGH, row);
		}
	}
	gi.LocClient_Print(player, PRINT_HIGH, "Ranked runs need every item type on, including stimpacks and shards.");
	gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("All runs are saved to {}.", RECORDS_FILE).data());
	gi.LocClient_Print(player, PRINT_HIGH, "=====================================\n");
}

// trainer_runs [map]: the current map's top runs, or another map's from the file.
void Cmd_TrainerRuns_f(edict_t *ent)
{
	MapTrainer_RecordsPrint(ent, gi.argc() > 1 ? gi.argv(1) : nullptr);
}
