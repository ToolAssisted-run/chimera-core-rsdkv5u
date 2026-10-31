/* game-state.cpp - what the tools see of the game: chimera docs/game-cores.md.
 *
 * Three memory domains, all the core's own blocks:
 *
 *   - "Game State": copied from the engine after every step - the engine's
 *     mode, the stage's mode, the scene and its timer. Read-only, as the
 *     engine derives them.
 *   - "Players": the first two players, their numbers only. In the v4 games
 *     (Sonic 1, Sonic 2) a player is an object, and players 1 and 2 are
 *     objects 0 and 1; in the v3 game (Sonic CD) the engine keeps a player
 *     list of its own. Copied out after every step and back in before the
 *     next, so a poke lands when the next step begins.
 *   - "Globals": the game's global variables (the scripts' own, as the
 *     game's GameConfig names them), one 32-bit value each, in the engine's
 *     order. Copied out and back in the same way. The property table lists
 *     each one by its name: "Global.<name>".
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "RSDK/Core/RetroEngine.hpp"

#include "rsdk-driver.h"

using namespace RSDK;

namespace {

struct __attribute__((packed)) GameState
{
	uint64_t frames;          /* 0 */
	uint8_t version;          /* 8: the engine's mode: 3 or 4 */
	uint8_t game_mode;        /* 9: Legacy::gameMode */
	uint8_t stage_mode;       /* 10: Legacy::stageMode */
	uint8_t category;         /* 11: sceneInfo.activeCategory */
	uint16_t list_pos;        /* 12: sceneInfo.listPos */
	uint8_t time_enabled;     /* 14 */
	uint8_t pad;              /* 15 */
	uint32_t time_ms;         /* 16: the stage timer, in milliseconds */
	int32_t act_id;           /* 20 */
	char scene_name[32];      /* 24 */
	char scene_folder[16];    /* 56 */
	char scene_id[8];         /* 72 */
};
static_assert(sizeof(GameState) == 80, "GameState layout");

struct __attribute__((packed)) PlayerState
{
	int32_t x, y;                     /* 0, 4: 16.16 fixed point */
	int32_t x_velocity, y_velocity;   /* 8, 12 */
	int32_t speed;                    /* 16: along the ground */
	int32_t angle;                    /* 20 */
	uint8_t direction;                /* 24 */
	uint8_t collision_mode;           /* 25 */
	uint8_t collision_plane;          /* 26 */
	uint8_t pad;                      /* 27 */
};
static_assert(sizeof(PlayerState) == 28, "PlayerState layout");

GameState gs;
PlayerState ps[2];
int32_t globals[LEGACY_GLOBALVAR_COUNT];

/* the v3 game's player p, as its entity number gives it */
Legacy::v3::Entity *v3_entity(int p)
{
	const int32 no = Legacy::v3::playerList[p].entityNo;
	if (no < 0 || no >= LEGACY_v3_ENTITY_COUNT) return NULL;
	return &Legacy::v3::objectEntityList[no];
}

void copy_name(char *dst, size_t n, const char *src)
{
	memset(dst, 0, n);
	if (src) strncpy(dst, src, n - 1);
}

} // namespace

extern "C" {

void gamestate_from_game(void)
{
	gs.frames = rsdkdrv_frames();
	gs.version = (uint8_t)engine.version;
	gs.game_mode = (uint8_t)Legacy::gameMode;
	gs.stage_mode = (uint8_t)Legacy::stageMode;
	gs.category = sceneInfo.activeCategory;
	gs.list_pos = sceneInfo.listPos;
	gs.time_enabled = Legacy::timeEnabled ? 1 : 0;
	gs.time_ms = (uint32_t)(Legacy::stageMinutes * 60000 + Legacy::stageSeconds * 1000 + Legacy::stageMilliseconds * 10);
	gs.act_id = Legacy::actID;
	copy_name(gs.scene_name, sizeof gs.scene_name, NULL);
	copy_name(gs.scene_folder, sizeof gs.scene_folder, NULL);
	copy_name(gs.scene_id, sizeof gs.scene_id, NULL);
	if (sceneInfo.listData && sceneInfo.listCategory && sceneInfo.listPos < sceneInfo.listCategory[sceneInfo.activeCategory].sceneOffsetEnd)
	{
		const SceneListEntry &e = sceneInfo.listData[sceneInfo.listPos];
		copy_name(gs.scene_name, sizeof gs.scene_name, e.name);
		copy_name(gs.scene_folder, sizeof gs.scene_folder, e.folder);
		copy_name(gs.scene_id, sizeof gs.scene_id, e.id);
	}
	for (int p = 0; p < 2; p++)
	{
		PlayerState &o = ps[p];
		memset(&o, 0, sizeof o);
		if (engine.version == 4)
		{
			const Legacy::v4::Entity &e = Legacy::v4::objectEntityList[p];
			o.x = e.xpos;
			o.y = e.ypos;
			o.x_velocity = e.xvel;
			o.y_velocity = e.yvel;
			o.speed = e.speed;
			o.angle = e.angle;
			o.direction = e.direction;
			o.collision_mode = e.collisionMode;
			o.collision_plane = e.collisionPlane;
		}
		else if (engine.version == 3)
		{
			const Legacy::v3::Player &pl = Legacy::v3::playerList[p];
			o.x = pl.XPos;
			o.y = pl.YPos;
			o.x_velocity = pl.XVelocity;
			o.y_velocity = pl.YVelocity;
			o.speed = pl.speed;
			o.angle = pl.angle;
			const Legacy::v3::Entity *e = v3_entity(p);
			o.direction = e ? e->direction : 0;
			o.collision_mode = pl.collisionMode;
			o.collision_plane = pl.collisionPlane;
		}
	}
	for (int i = 0; i < LEGACY_GLOBALVAR_COUNT; i++)
		globals[i] = i < Legacy::globalVariablesCount ? Legacy::globalVariables[i].value : 0;
}

/* the blocks back into the engine before a step: what a poke or a freeze
 * wrote there */
void gamestate_to_game(void)
{
	for (int p = 0; p < 2; p++)
	{
		const PlayerState &o = ps[p];
		if (engine.version == 4)
		{
			Legacy::v4::Entity &e = Legacy::v4::objectEntityList[p];
			e.xpos = o.x;
			e.ypos = o.y;
			e.xvel = o.x_velocity;
			e.yvel = o.y_velocity;
			e.speed = o.speed;
			e.angle = o.angle;
			e.direction = o.direction;
		}
		else if (engine.version == 3)
		{
			Legacy::v3::Player &pl = Legacy::v3::playerList[p];
			pl.XPos = o.x;
			pl.YPos = o.y;
			pl.XVelocity = o.x_velocity;
			pl.YVelocity = o.y_velocity;
			pl.speed = o.speed;
			pl.angle = o.angle;
			Legacy::v3::Entity *e = v3_entity(p);
			if (e) e->direction = o.direction;
		}
	}
	for (int i = 0; i < Legacy::globalVariablesCount && i < LEGACY_GLOBALVAR_COUNT; i++)
		Legacy::globalVariables[i].value = globals[i];
}

int rsdkdrv_domain_count(void) { return 3; }

const char *rsdkdrv_domain_name(int i)
{
	static const char *const names[] = { "Game State", "Players", "Globals" };
	return i >= 0 && i < 3 ? names[i] : "";
}

uint8_t *rsdkdrv_domain_ptr(int i)
{
	switch (i)
	{
	case 0: return (uint8_t *)&gs;
	case 1: return (uint8_t *)ps;
	case 2: return (uint8_t *)globals;
	}
	return NULL;
}

int64_t rsdkdrv_domain_size(int i)
{
	switch (i)
	{
	case 0: return (int64_t)sizeof gs;
	case 1: return (int64_t)sizeof ps;
	case 2: return (int64_t)sizeof globals;
	}
	return 0;
}

int rsdkdrv_domain_writable(int i) { return i == 1 || i == 2; }

/* The table is made once, after Init: the global variables' names are the
 * game's GameConfig's, which the engine has read by then. A name goes into
 * the JSON with what JSON needs escaped; one that repeats an earlier name is
 * left out (the engine finds a global by its first name too). */
const char *rsdkdrv_game_properties(void)
{
	static char json[65536];
	if (json[0]) return json;
	int n = 0;
#define P(...) n += snprintf(json + n, sizeof json - (size_t)n, __VA_ARGS__)
#define GS(name, off, type, extra) \
	P("    { \"name\": \"%s\", \"domain\": \"Game State\", \"offset\": %d, \"type\": \"%s\", \"writable\": false%s },\n", name, off, type, extra)
	P("{\n  \"gameTimer\": \"Stage.Time Ms\",\n  \"properties\": [\n");
	GS("Engine.Frames", 0, "u64", ", \"group\": \"Engine\"");
	GS("Engine.Version", 8, "u8", ", \"group\": \"Engine\", \"description\": \"property.engine_version\"");
	GS("Engine.Mode", 9, "u8", ", \"group\": \"Engine\", \"values\": { \"0\": \"Developer Menu\", \"1\": \"Main Game\", \"2\": \"Init Developer Menu\", "
	   "\"3\": \"Wait\", \"4\": \"Script Error\", \"5\": \"Init Pause\", \"6\": \"Exit Pause\", \"7\": \"End Game\", \"8\": \"Reset Game\" }");
	GS("Stage.Mode", 10, "u8", ", \"group\": \"Stage\"");
	GS("Stage.Category", 11, "u8", ", \"group\": \"Stage\"");
	GS("Stage.List Position", 12, "u16", ", \"group\": \"Stage\"");
	GS("Stage.Time Enabled", 14, "bool", ", \"group\": \"Stage\"");
	GS("Stage.Time Ms", 16, "u32", ", \"group\": \"Stage\", \"description\": \"property.scene_time_ms\"");
	GS("Stage.Act", 20, "s32", ", \"group\": \"Stage\"");
	P("    { \"name\": \"Stage.Name\", \"domain\": \"Game State\", \"offset\": 24, \"type\": \"string\", \"length\": 32, \"writable\": false, \"group\": \"Stage\" },\n");
	P("    { \"name\": \"Stage.Folder\", \"domain\": \"Game State\", \"offset\": 56, \"type\": \"string\", \"length\": 16, \"writable\": false, \"group\": \"Stage\" },\n");
	P("    { \"name\": \"Stage.ID\", \"domain\": \"Game State\", \"offset\": 72, \"type\": \"string\", \"length\": 8, \"writable\": false, \"group\": \"Stage\" },\n");
	static const struct { const char *name; int off; const char *type; int writable; const char *desc; } k_player[] = {
		{ "X", 0, "s32", 1, "property.player.x" },
		{ "Y", 4, "s32", 1, "property.player.x" },
		{ "X Velocity", 8, "s32", 1, "" },
		{ "Y Velocity", 12, "s32", 1, "" },
		{ "Speed", 16, "s32", 1, "" },
		{ "Angle", 20, "s32", 1, "" },
		{ "Direction", 24, "u8", 1, "" },
		{ "Collision Mode", 25, "u8", 0, "" },
		{ "Collision Plane", 26, "u8", 0, "" },
	};
	for (int p = 0; p < 2; p++)
		for (size_t i = 0; i < sizeof k_player / sizeof k_player[0]; i++)
		{
			P("    { \"name\": \"Player %d.%s\", \"domain\": \"Players\", \"offset\": %d, \"type\": \"%s\", \"group\": \"Player %d\"%s",
			  p + 1, k_player[i].name, p * (int)sizeof(PlayerState) + k_player[i].off, k_player[i].type, p + 1,
			  k_player[i].writable ? "" : ", \"writable\": false");
			if (k_player[i].desc[0]) P(", \"description\": \"%s\"", k_player[i].desc);
			P(" },\n");
		}
	const int name_len = (int)sizeof Legacy::globalVariables[0].name;
	for (int i = 0; i < Legacy::globalVariablesCount && i < LEGACY_GLOBALVAR_COUNT; i++)
	{
		const char *name = Legacy::globalVariables[i].name;
		if (!name[0]) continue;
		int dup = 0;
		for (int k = 0; k < i && !dup; k++)
			dup = !strncmp(Legacy::globalVariables[k].name, name, (size_t)name_len);
		if (dup) continue;
		char esc[2 * 0x20 + 1];
		int e = 0;
		for (int c = 0; c < name_len && name[c]; c++)
		{
			const unsigned char ch = (unsigned char)name[c];
			if (ch < 0x20 || ch >= 0x7f) continue;
			if (ch == '"' || ch == '\\') esc[e++] = '\\';
			esc[e++] = (char)ch;
		}
		esc[e] = '\0';
		P("    { \"name\": \"Global.%s\", \"domain\": \"Globals\", \"offset\": %d, \"type\": \"s32\", \"group\": \"Globals\" },\n", esc, i * 4);
	}
	/* the last entry carries no comma */
	if (n >= 2 && json[n - 2] == ',')
	{
		json[n - 2] = '\n';
		json[--n] = '\0';
	}
	P("  ]\n}\n");
#undef GS
#undef P
	return json;
}

} // extern "C"
