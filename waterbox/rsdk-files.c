/* rsdk-files.c - every file the engine opens, answered by the core.
 *
 * The engine opens its files through SDL's file reader, and SDL and the
 * C library open them with fopen; the link wraps both SDL_RWFromFile and
 * fopen (sources.mk), so each open lands here:
 *
 *   - Data.rsdk, the game's data pack: the file the project brings as its
 *     firmware (waterbox.config; one firmware a game, the Game setting
 *     choosing, mounted under its id), copied once at Init into sealed memory -
 *     read-only after Init, and so in no savestate - and opened from there
 *     with fmemopen. The stream itself (its position, its buffer) is in the
 *     guest's heap, which a savestate carries; a host file handle would not
 *     survive a load.
 *   - Settings.ini: written by the core from the project's settings, so the
 *     engine starts as the project says and never as a settings file left on
 *     some disk says.
 *   - every file the engine or the game writes (the saves, the options,
 *     Settings.ini when the game writes it back): a
 *     file in guest memory, empty when the machine starts, kept by name and
 *     read back by name, and carried by savestates like the rest of it.
 *   - anything else: not there.
 *
 * Names are matched by their last part and without regard to case.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <SDL2/SDL.h>

#include <emulibc.h>
#include <waterbox_settings.h>
#include <waterbox_slots.h>

#include "rsdk-driver.h"

/* A text a person reads is a key of languages/en.json, each of its
 * arguments after a TEXT_ARG (chimera docs/translations.md). TEXT_ARG is a
 * string of its own so the byte after it is never read as more of its hex. */
#define TEXT_ARG "\x1f"

FILE *__real_fopen(const char *path, const char *mode);
int __real_fclose(FILE *f);

/* the data pack, sealed after Init */
static uint8_t *g_pack;
static size_t g_pack_size;

/* Settings.ini, as the core wrote it */
static char g_ini[4096];
static size_t g_ini_len;

/* the files written: a stream being written is an open_memstream, whose
 * buffer the C library hands back at fclose; the last one closed is the
 * file */
#define MEMFILES 32
struct mem_file
{
	char name[64];
	char *data;         /* the file's bytes (malloc'd: guest heap) */
	size_t size;
	char *writing;      /* open_memstream's, while a write is open */
	size_t writing_size;
	FILE *stream;
};
static struct mem_file g_files[MEMFILES];

/* ------------------------------------------------------------ settings */

static const char *const k_games[RSDK_GAME_COUNT] = { "sonic1", "sonic2", "soniccd" };
static const char *const k_firmware[RSDK_GAME_COUNT] = { "Sonic1u.rsdk", "Sonic2u.rsdk", "SonicCDu.rsdk" };

const char *rsdk_game_firmware(int game) { return game >= 0 && game < RSDK_GAME_COUNT ? k_firmware[game] : ""; }

void rsdk_settings_read(struct rsdk_settings *s)
{
	char str[16] = "sonic1";
	wbx_setting_str("game", str, (int)sizeof str);
	s->game = RSDK_GAME_SONIC1;
	for (int i = 0; i < RSDK_GAME_COUNT; i++)
		if (!strcmp(str, k_games[i])) s->game = i;
	snprintf(str, sizeof str, "en");
	wbx_setting_str("language", str, (int)sizeof str);
	static const char *const langs[] = { "en", "fr", "it", "de", "es", "ja", "ko", "zhs", "zh" };
	s->language = 0;
	for (int i = 0; i < (int)(sizeof langs / sizeof langs[0]); i++)
		if (!strcmp(str, langs[i])) s->language = i;
	s->screen_width = (int)wbx_setting_long("screenWidth", 424);
	s->plus = wbx_setting_bool("plus", 0);
	s->random_seed = wbx_setting_long("randomSeed", 0);
	s->dev_menu = wbx_setting_bool("devMenu", 0);
}

/* the engine's settings file (UserCore.cpp LoadSettingsINI reads it): the
 * keys it knows, the values the project's settings give. What is not a
 * setting is upstream's default, except where the default would make the
 * picture or the run depend on the host: windowed, no VSync, no shader, no
 * focus pause, no developer menu. gameType=1 is the Origins release: the
 * scripts of Origins' packs ask for it (engine.releaseType "USE_ORIGINS"). The keyboard maps are upstream's own
 * defaults (Windows key codes); the core sets the keyboard's state itself
 * (rsdk-driver.cpp). */
static void write_ini(const struct rsdk_settings *s)
{
	int n = snprintf(g_ini, sizeof g_ini,
		"[Game]\n"
		"dataFile=Data.rsdk\n"
		"devMenu=%s\n"
		"language=%d\n"
		"region=-1\n"
		"faceButtonFlip=n\n"
		"enableControllerDebugging=n\n"
		"disableFocusPause=y\n"
		"fastForwardSpeed=8\n"
		"txtScripts=n\n"
		"gameType=1\n"
		"gameLogic=Game\n"
		"[Video]\n"
		"windowed=y\n"
		"border=y\n"
		"exclusiveFS=n\n"
		"vsync=n\n"
		"tripleBuffering=n\n"
		"pixWidth=%d\n"
		"winWidth=%d\n"
		"winHeight=240\n"
		"fsWidth=0\n"
		"fsHeight=0\n"
		"refreshRate=60\n"
		"shaderSupport=n\n"
		"screenShader=0\n"
		"maxPixWidth=%d\n"
		"[Audio]\n"
		"streamsEnabled=y\n"
		"streamVolume=1.0\n"
		"sfxVolume=1.0\n",
		s->dev_menu ? "y" : "n", s->language, s->screen_width, s->screen_width, s->screen_width);
	g_ini_len = (size_t)n;
}

/* ------------------------------------------------------------ the pack */

/* the engine version the pack's header names ("RSDKv3", "RSDKv4",
 * "RSDKv5"), which the engine's DetectEngineVersion reads to choose its mode */
static int g_pack_version;
int rsdk_pack_version(void) { return g_pack_version; }

/* the game's data pack: the project's firmware for the chosen game, mounted
 * under its id (waterbox.config) */
static int read_pack(const struct rsdk_settings *s, char *err, int errsize)
{
	const char *name = rsdk_game_firmware(s->game);
	FILE *f = __real_fopen(name, "rb");
	if (!f)
	{
		snprintf(err, (size_t)errsize, "error.game_s_data_file_missing" TEXT_ARG "%s", name);
		return 0;
	}
	fseek(f, 0, SEEK_END);
	const long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size <= 6)
	{
		fclose(f);
		snprintf(err, (size_t)errsize, "error.is_not_a_retro_engine_data" TEXT_ARG "%s" TEXT_ARG "%ld", name, size);
		return 0;
	}
	g_pack = (uint8_t *)alloc_sealed((size_t)size);
	if (!g_pack)
	{
		fclose(f);
		snprintf(err, (size_t)errsize, "error.is_too_large_bytes_for_the" TEXT_ARG "%s" TEXT_ARG "%ld", name, size);
		return 0;
	}
	const size_t got = fread(g_pack, 1, (size_t)size, f);
	fclose(f);
	if (got != (size_t)size)
	{
		snprintf(err, (size_t)errsize, "error.could_not_be_read_whole" TEXT_ARG "%s", name);
		return 0;
	}
	/* the engine takes a file that is not a pack for no data at all and
	 * starts its developer menu: the core refuses it instead. The pack is
	 * the v5 engine's layout; its header names the engine version whose
	 * game it holds. The standalone releases' packs are refused with the
	 * core that plays them (user-decided, 2026-10-31; docs/PLAN.md): the
	 * 2013 Sonic 1 and 2 start with "RSDKvB", files encrypted v4's way
	 * (rsdkv4); the 2011 Sonic CD has no header at all (rsdkv3), which the
	 * text for a file that is not a pack names. */
	if (memcmp(g_pack, "RSDKvB", 6) == 0)
	{
		snprintf(err, (size_t)errsize, "error.pack_is_from_2013_release" TEXT_ARG "%s", name);
		return 0;
	}
	if (memcmp(g_pack, "RSDKv", 5) != 0 || g_pack[5] < '3' || g_pack[5] > '5')
	{
		snprintf(err, (size_t)errsize, "error.is_not_a_retro_engine_pack" TEXT_ARG "%s", name);
		return 0;
	}
	g_pack_version = g_pack[5] - '0';
	g_pack_size = (size_t)size;
	return 1;
}

/* The project's save data (the savedata slot): the files the
 * game writes (its save file and the rest) as Export Save Data wrote them, put
 * in place here at Init, before the seal, so they are part of the machine as
 * it starts. Settings.ini is the project's settings, not save data. */
static int seed_saves(char *err, int errsize)
{
	char name[256];
	for (int i = 0; wbx_slot_name("savedata", i, name, (int)sizeof name); i++)
	{
		if (!strcasecmp(name, "Settings.ini") || strlen(name) >= sizeof g_files[0].name)
		{
			snprintf(err, (size_t)errsize, "error.save_data_is_not_one_this" TEXT_ARG "%s" TEXT_ARG "%d",
			         name, (int)sizeof g_files[0].name - 1);
			return 0;
		}
		struct mem_file *mf = NULL;
		for (int k = 0; k < MEMFILES && !mf; k++)
			if (!g_files[k].name[0]) mf = &g_files[k];
		if (!mf)
		{
			snprintf(err, (size_t)errsize, "error.save_data_has_more_than" TEXT_ARG "%d", MEMFILES);
			return 0;
		}
		FILE *f = __real_fopen(name, "rb");
		if (!f)
		{
			snprintf(err, (size_t)errsize, "error.save_data_could_not_be" TEXT_ARG "%s", name);
			return 0;
		}
		size_t cap = 0, n = 0;
		char *data = NULL;
		for (;;)
		{
			if (n == cap)
			{
				cap = cap ? cap * 2 : 65536;
				data = (char *)realloc(data, cap);
			}
			const size_t got = fread(data + n, 1, cap - n, f);
			n += got;
			if (got == 0) break;
		}
		__real_fclose(f);
		snprintf(mf->name, sizeof mf->name, "%s", name);
		mf->data = data;
		mf->size = n;
	}
	return 1;
}

int rsdk_files_load(const struct rsdk_settings *s, char *err, int errsize)
{
	write_ini(s);
	return read_pack(s, err, errsize) && seed_saves(err, errsize);
}

/* ------------------------------------------------------------ export */

static int g_export[MEMFILES];
static int g_export_count;

int rsdk_saves_count(void)
{
	g_export_count = 0;
	for (int i = 0; i < MEMFILES; i++)
		if (g_files[i].name[0] && g_files[i].data && g_files[i].size && strcasecmp(g_files[i].name, "Settings.ini") &&
		    strcasecmp(g_files[i].name, "log.txt"))
			g_export[g_export_count++] = i;
	return g_export_count;
}
const char *rsdk_saves_name(int index) { return index >= 0 && index < g_export_count ? g_files[g_export[index]].name : ""; }
int64_t rsdk_saves_size(int index) { return index >= 0 && index < g_export_count ? (int64_t)g_files[g_export[index]].size : 0; }
const uint8_t *rsdk_saves_data(int index) { return index >= 0 && index < g_export_count ? (const uint8_t *)g_files[g_export[index]].data : NULL; }

/* ------------------------------------------------------------ fopen */

static const char *base_name(const char *path)
{
	const char *b = strrchr(path, '/');
	return b ? b + 1 : path;
}

static int writes(const char *mode) { return strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+'); }

static struct mem_file *find_file(const char *name)
{
	for (int i = 0; i < MEMFILES; i++)
		if (g_files[i].name[0] && !strcasecmp(g_files[i].name, name)) return &g_files[i];
	return NULL;
}

static FILE *open_written(const char *name)
{
	struct mem_file *mf = find_file(name);
	if (!mf)
	{
		for (int i = 0; i < MEMFILES && !mf; i++)
			if (!g_files[i].name[0]) mf = &g_files[i];
		if (!mf || strlen(name) >= sizeof mf->name) { errno = ENOSPC; return NULL; }
		snprintf(mf->name, sizeof mf->name, "%s", name);
	}
	if (mf->stream) { errno = EBUSY; return NULL; } /* one writer at a time */
	free(mf->writing);
	mf->writing = NULL;
	mf->writing_size = 0;
	mf->stream = open_memstream(&mf->writing, &mf->writing_size);
	return mf->stream;
}

FILE *__wrap_fopen(const char *path, const char *mode)
{
	if (!path || !mode) { errno = EINVAL; return NULL; }
	/* the core's own channels, read by the miniBox kit at Init */
	if (!strcmp(path, "settings") || !strcmp(path, "slots")) return __real_fopen(path, mode);
	const char *name = base_name(path);
	if (writes(mode)) return open_written(name);
	struct mem_file *mf = find_file(name);
	if (mf)
	{
		if (!mf->data || !mf->size) { errno = ENOENT; return NULL; }
		return fmemopen(mf->data, mf->size, "rb");
	}
	if (!strcasecmp(name, "Settings.ini") && g_ini_len) return fmemopen(g_ini, g_ini_len, "rb");
	if (!strcasecmp(name, "Data.rsdk") && g_pack) return fmemopen(g_pack, g_pack_size, "rb");
	errno = ENOENT;
	return NULL;
}

/* The engine opens its files through SDL (Reader.hpp: FileIO is an
 * SDL_RWops), and SDL's SDL_RWFromFile refuses a stream that is not a
 * regular file or a pipe - a memory stream is neither, having no file
 * descriptor. So SDL is answered here too, with the same stream wrapped as
 * SDL would wrap a file's: closing it closes the stream (and a written
 * file's close, below, keeps its bytes). */
SDL_RWops *__wrap_SDL_RWFromFile(const char *file, const char *mode)
{
	FILE *f = __wrap_fopen(file, mode);
	if (!f)
	{
		SDL_SetError("Couldn't open %s", file ? file : "(null)");
		return NULL;
	}
	return SDL_RWFromFP(f, SDL_TRUE);
}

/* a file written and closed: its bytes become the file */
int __wrap_fclose(FILE *f)
{
	for (int i = 0; i < MEMFILES; i++)
	{
		struct mem_file *mf = &g_files[i];
		if (f && f == mf->stream)
		{
			const int r = __real_fclose(f);
			mf->stream = NULL;
			free(mf->data);
			mf->data = mf->writing;
			mf->size = mf->writing_size;
			mf->writing = NULL;
			mf->writing_size = 0;
			return r;
		}
	}
	return __real_fclose(f);
}
