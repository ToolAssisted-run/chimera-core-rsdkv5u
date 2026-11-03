# Plan and design log - Retro Engine v5U (Sonic Origins) core

Dated entries; the newest decisions are at the end of each section.

## The request (2026-10-31)

Chimera issue 409 asks for "RSDKv5-u (Ultimate)" for Sonic Origins and
links https://github.com/thesupersonic16/RSDKv5u-Origins. The owner asked
for the core requests to be built in local repositories only, until the
owner has checked the work: nothing pushed, no comment on the issue, no
change to Chimera. The owner supplies game data on request.

Chimera already has three Retro Engine cores, all published: rsdkv3 (Sonic
CD, its 2011 remaster), rsdkv4 (the 2013 remasters of Sonic 1 and 2) and
rsdkv5 (Sonic Mania). This repository was started as a copy of rsdkv5's
files and follows its design.

## What the requested fork is (2026-10-31)

RSDKv5u-Origins (pinned for this study at 74b191d, 2024-09-22, the last
commit) is a fork of the RSDKv5 decompilation turned into a mod for the
Windows release of Sonic Origins. It is not a program of its own:

- It is a DLL that a mod loader (HedgeModManager's) loads into
  SonicOrigins.exe. `RSDKv5/main.cpp` exports `Init` for the mod loader and
  hooks `D3D11CreateDevice` to start the engine in place of Origins' own.
- It finds functions inside SonicOrigins.exe by byte patterns
  (`Signatures/scans.bin`, `SigScan.cpp`, `Symbols.cpp`) and writes over
  some of the executable's code (`Patches.cpp`).
- Sonic 3 & Knuckles' game logic is not in the fork: `SigLinkGameLogic()`
  finds the logic Sega compiled into SonicOrigins.exe and links the engine
  to it. `S3K.cpp` adds Origins features around it (coins, mirror mode,
  achievements, loop points).
- Sound is Origins' own (Hedgehog Engine 2); Steam and Epic are called
  through their DLLs.

So the fork cannot run in Chimera's sandbox: it needs the commercial Windows
executable's own code, Direct3D 11 and the Windows API, and miniBox runs a
Linux guest built from source. The request as written is not feasible.

## What is feasible: Sonic Origins' Sonic 1, Sonic 2 and Sonic CD

Sonic Origins runs Sonic 1, Sonic 2 and Sonic CD on the same "Ultimate"
engine (RSDKv5U) in its legacy modes: Sonic 1 and 2 as v4 games, Sonic CD
as a v3 game. Their game logic is scripts inside their data packs, not a
program. The RSDKv5 decompilation itself (RSDKModding/RSDKv5-Decompilation,
the project the fork was made from) builds as v5U by default and plays
those packs in its legacy modes: `Reader.cpp DetectEngineVersion` reads the
engine version from the pack's header ("RSDKv3", "RSDKv4"), and the
scripts' Origins additions are in it (`engine.releaseType`
"USE_ORIGINS", the Origins notify callbacks in `RetroEnginev3.cpp`). Its
FAQ describes playing Origins' packs with it.

So this core is the upstream decompilation built as v5U, with no game
logic compiled in, playing those three games' packs. Sonic 3 & Knuckles is
not played: its logic exists only inside SonicOrigins.exe, and no
decompilation of it is public.

## Feasibility (2026-10-31)

- **Licence.** Upstream pinned at 191d7ae (2026-10-03, the master branch
  then; v1.1.1 and 13 commits, among them fixes to the v3 mode). Its
  LICENSE.md is the "RSDKv5(U) DECOMPILATION SOURCE CODE LICENSE v2.1",
  byte for byte the text at v1.1.1 (the rsdkv5 core's pin) and at the
  fork's 74b191d. It allows copying, modifying, contributing and
  distributing for public or private use, as long as: every pre-built
  executable given to anyone is built with the downloadable content off by
  default (a setting is allowed if it is off by default); nothing is used
  for profit; the decompilation and the original are credited and not
  claimed; no asset of a game the repository does not provide is
  distributed; the licence is in every modified copy of the source, and
  every fork follows it. It is not a free-software licence (no commercial
  use). The existing rsdk cores handled it this way, and so does this one:
  `RSDK_AUTOBUILD` defined (DLC off), a Plus Content setting off by default
  (user-decided for rsdkv5 on 2026-10-17), `commercialUse: no` in the
  package, the authors credited, LICENSE-RSDK.txt in the repository and the
  package, no game data anywhere. The owner accepted the same terms for
  this core (see "The owner's decisions", user-decided, 2026-10-31).
- **One thread on one clock.** As rsdkv5: the engine's main loop is one
  frame a pass (patches/0001 splits it into start-up, frame and shutdown);
  the only thread, the music loader, is run at once; the clocks are the
  frame count.
- **The picture.** The engine's software renderer into its screens, which
  SDL 2's software renderer puts in a window the core reads back. No GPU.
- **Recompiler.** None.
- **What the user brings.** The three data packs from a Sonic Origins
  installation. The names the core uses are Sonic1u.rsdk, Sonic2u.rsdk and
  SonicCDu.rsdk, from the fork's own "retro/Sonic3ku.rsdk" naming: not yet
  checked against a real installation (see "Where this stands").
- **What CI can run.** Two small games written from scratch for the tests
  (`waterbox/tests/make-test-game.py`): one for the v4 mode, one for the v3
  mode, each a pack in the v5 layout with the older engine's header, as
  Origins' are. Everything but the real games runs on them.
- **Memory.** As rsdkv5: the engine's pools on the heap, the data pack in
  sealed memory (read-only after Init, in no savestate). A state of a test
  game is 4.6 to 6 MB.

## How the core is built (2026-10-31)

The rsdkv5 core's integration, with Sonic Mania taken out:

- **Upstream** `extern/RSDKv5` at 191d7ae, with patches:
  - 0001, 0002: rsdkv5's (one frame at a time; the Plus setting in the
    dummy user core), applied unchanged.
  - 0003: the Plus setting also reaches the legacy modes. Upstream's
    autobuild code forces `game.hasPlusDLC` to 0 and stops Amy and
    Knuckles (Sonic CD) and Amy's hammer; with the setting on, those
    follow it instead. Off by default.
  - 0004: `RSDK::ProcessInput` counts its calls, so a frame that did not
    read the controls is a lag frame. (rsdkv5 decided lag from the v5
    scene state, which the legacy modes do not use.)
- **No game logic.** `RETRO_STANDALONE=0` with `linkGameLogic` NULL. A pack
  whose header says RSDKv5 is refused with a reason (its game is a program
  this core does not have); a file that is not a pack is refused too.
- **Firmware, one a game.** A Game setting (sonic1, sonic2, soniccd)
  chooses which of Sonic1u.rsdk, Sonic2u.rsdk, SonicCDu.rsdk is the pack.
  The core serves it to the engine as Data.rsdk from sealed memory.
- **Settings file.** Written by the core, with `gameType=1` (the Origins
  release, which Origins' scripts ask for).
- **Settings.** Game, Language, Screen Width (320 to 424), Plus Content
  (off), Random Seed, Developer Menu. Region, Players (3 and 4) and Player
  Name from rsdkv5 are gone: they are Sonic Mania's.
- **Controls.** Two players' twelve buttons and Dev Menu (live only with
  the Developer Menu setting), 25 in all.
- **Properties.** Three domains: Game State (engine version and mode, the
  stage's mode, category, position, timer, act, names; read only),
  Players (players 1 and 2: in v4 objects 0 and 1, in v3 the engine's
  player list; position, speeds, angle, direction), and Globals (the
  game's global variables, listed by the names the game's GameConfig gives
  them, "Global.<name>"). The game timer is Stage.Time Ms.
- **Texts.** languages/*.json, 47 keys in each of the 26 languages; the
  new texts were translated by Claude (as Chimera's own are) and have not
  been read by speakers of each language.

## Measured (2026-10-31)

On the two test games, 100 frames of `waterbox/tests/test.movie`, with
Chimera at 3f8ea7e2 (~/chimera-scratch-bump) and its miniBox:

- Core tests (`waterbox/run-gate.sh`): 26 passed, 0 failed, 3 skipped (the
  three Origins packs, not given). For each test game: native = sandbox
  (picture, sound, frame lengths, lag, memory), rerecord = straight (state
  5.99 MB for v4, 4.60 MB for v3), session at frame 75 = straight, four
  presses of A counted, one lag frame (the stage's load), the save written
  and read back (also across a session). Turbo, pokes (Player 1.X and
  Global.test.presses in both modes), random seed, screen width 320, the
  game setting, the Plus setting (game.hasPlusDLC 0 off, 1 on), four
  refusals, the clock, teeth, firmware names.
- Frontend tests (`waterbox/tests/run-frontend.sh`, Chimera's Qt window run
  headless and chimera-run): 11 of 11 after the refusal text was corrected
  (10 of 11 on the first run: the test expected rsdkv5's wording). The
  project made by chimera-run and the project opened in the window end
  with the reference's Game State, Players and picture; the v3 test game
  through chimera-run; the stage timer in the saved project (00:01.650);
  game.list (34 properties), game.get/set on Player 1.X and
  Global.test.presses; Engine.Version refused as read only; the package's
  25 bindings adopted.
- Chimera's contract tests (`test-packages` on a folder holding only this
  package): 21 passed, 0 failed.
- Broken on purpose and seen to fail: the lag count (input fails in both
  modes), the copy of the globals back into the game (pokes fail in both
  modes), the v5 refusal, the Plus setting in v4 (settings: plus fails).
- Not tried: any of Sonic Origins' own packs. No game of Sonic Origins is
  said to work.

## Traps

- **A control read with no index** in a legacy script reads the controller
  numbered as the object reading it (`ScriptLegacyv4.cpp`, VARARR_NONE is
  objectEntityPos). Right for a game's players (objects 0 and 1), wrong for
  a test object placed further on, which then never sees a press. The test
  scripts read keyDown[0] and keyPress[0].
- **Included sources are not dependencies.** The engine compiles its
  legacy code by `#include`-ing .cpp files into others (RetroEngine.cpp
  includes RetroEnginev4.cpp). The makefiles know only the outer file, so
  an edit to an included one does not rebuild; touch the outer file. The
  patch series is a dependency of every object, so patches do rebuild.
- **Chimera's window needs Qt's libraries**: on this machine
  `LD_LIBRARY_PATH=$HOME/qt/6.12.0/gcc_64/lib` for the frontend test.
- The harness's `--dump-domain` keeps only the last one given.

## The owner's decisions (user-decided, 2026-10-31)

The previous round put four questions to the owner (a new core or a part
of rsdkv5, the name, the licence, the sound) and recommended an answer to
each. The owner accepted every recommendation:

- **A new core.** rsdkv5u is its own game core in its own repository, as
  built, not a machine added to the published rsdkv5 core. The rsdkv5 core
  compiles Sonic Mania's logic in, pins the engine where Sonic Mania needs
  it and has Sonic Mania's settings and properties; this one has none of
  them.
- **The name.** The core is rsdkv5u (its coreName). The system is named
  "Sonic Origins (Sonic 1, Sonic 2, Sonic CD)", which waterbox.config
  already says. Sonic 3 & Knuckles is not in it.
- **The licence.** The same RSDKv5(U) decompilation licence terms as the
  rsdkv3, rsdkv4 and rsdkv5 cores, with the Plus Content setting off by
  default. The setting also covers what the licence's "downloadable
  content" means in the older games: Amy, and Knuckles in Sonic CD (patch
  0003). This is how the core was built; nothing changed.
- **Sound.** Decided after Sonic Origins' own packs have been tried; the
  owner will provide them. No second sound pack is built now.
- **Publishing.** Only after the Origins packs have been tried: no public
  repository, no entry in Chimera's list of cores and no release before
  then.

None of these changed the core's code or declarations: the core already
was what the recommendations described.

## Names are never translated (user-decided, 2026-10-31)

The owner's rule for every language table: a name - Chimera's, a
program's, a game's, a character's, a machine's, a company's, this core's
coreName - is written as it is in every language. A language whose grammar
puts endings on names says the sentence with a common noun beside the
name instead ("Retro Engine -moottorin", "Sonic Origins koleksiyonunun",
"a Sonic CD jatekban"). No case ending, genitive "s" or other letters are
glued to a name; a particle in another script (Korean's, Japanese's) is
fine.

What was wrong in the 26 tables: the check reports 51 problems in 27 texts
of 9 languages.

- Finnish, Hungarian and Turkish put endings on Sonic Origins, Origins
  Plus, Sonic CD and Retro Engine ("Sonic Originsin", "Sonic CD:ssa",
  "Sonic Originsbol", "Sonic Origins'ten"); Czech and Polish on Knuckles
  ("Knucklese", "Knucklesa").
- Japanese, Korean, Russian and Ukrainian wrote Amy and Knuckles in their
  own scripts. They are now written as they are.

`waterbox/tests/check-names.py` is the check, and the gate runs it (the
test "names"). It knows Chimera's names (from Chimera's
tools/check-proper-nouns.py) and this core's: rsdkv5u, Retro Engine,
RSDKv3/v4/v5, Sonic Origins, Origins Plus, the games, the characters,
Evening Star, Sega, SDL. It says when a name an English text holds is not
in the same text of another language as it is, and when Latin letters are
glued to a name or a short ending (up to four letters) is joined to it by
a hyphen, colon or apostrophe. A hyphen and a longer word is a compound
with a common noun ("Plus-Inhalte") and passes. With one Turkish text left
bent the gate failed on it (26 passed, 1 failed); after it was fixed, 27
passed. Chimera's own check (tools/check-proper-nouns.py, pointed at
waterbox) and tools/check-core-texts.py also pass on the tables.

## A fresh clone (2026-10-31)

A `git clone` of this repository, `git submodule update --init
--recursive`, then the commands of docs/BUILDING.md, with Chimera main
cb46d2b7 and its miniBox (7216594): the gate built everything from
nothing (355 objects) and passed 27, failed 0, skipped 3 (the Origins
packs); build-package.sh made the package; the frontend test passed 11 of
11; Chimera's contract tests 21 passed, 0 failed. Its core.wbx is
byte for byte the one built in this checkout.

One thing it found: the package's build.json recorded the clone's origin,
a folder on this machine (/home/<user>/...), so a package built from a
local clone carried the builder's path and user name. build-package.sh now
records the origin only when it is a repository on the network
(https://... or git@...); tried with both (empty for the folder, the
GitHub address for that).

## The standalone packs (2026-10-31, third round)

The owner gave three packs of the standalone PC releases, not Sonic
Origins': Sonic 1 (2013) and Sonic 2 (2013), each a data.rsdk for the
Retro Engine v4, and Sonic CD (2011), for the v3, with data.rsdk,
data_steam.rsdk (the same file), data_mobile.rsdk and four videos. Only the
data files were taken from the zips; nothing else in them was run or
extracted. These are the packs the published rsdkv4 and rsdkv3 cores play.

**The core refuses them as they come**, all three with "not a Retro Engine
data pack". The engine could not read them either:

- v5U reads one file layout, the v5 one: "RSDK", "v", a version letter, a
  table of files found by the MD5 of their names (`Reader.cpp`
  LoadDataPack). `DetectEngineVersion` takes the version letter: 3, 4 or 5.
- The 2013 packs are in that layout but headed "RSDKvB", which the engine
  does not take for v4, and their files are encrypted with v4's keys (made
  from each file's size); v5U decrypts with v5's (made from the file's
  name). The engine read garbage and crashed in LoadGameConfig.
- The 2011 Sonic CD packs have no header at all: the v3 engine's own layout
  (folders, then files, names and data encrypted v3's way). v5U has no
  reader for it.
- Sonic CD's PC packs (data.rsdk, data_steam.rsdk) hold the scripts in the
  PC form (Data/Scripts/ByteCode/GS000.bin and the rest). v5U's v3 mode
  reads only the mobile form (GlobalCode.bin and a file a stage), so even
  converted they show a black screen. data_mobile.rsdk has the mobile form.

**Converted for the tests, they play.** `waterbox/tests/convert-pack.py`
rewrites a pack in the v5 layout with every file decrypted. The core then
takes the copy (headed RSDKv4 or RSDKv3) like any pack. What was seen, by
eye, frame by frame, on the copies of Sonic 1 (2013), Sonic 2 (2013) and
Sonic CD's data_mobile.rsdk:

- Each boots: the SEGA screen, the games' logos, the title, and left alone
  the attract demos (Green Hill and Marble Zone in Sonic 1, Palmtree Panic
  in Sonic CD).
- **The title does not lead into the game.** Start and A at the title do
  nothing in Sonic 1 and 2; in Sonic CD the "TOUCH TO START" text goes and
  the title stays. The releases' menus are their own engines' code (the
  2013 remasters' menus are native objects of the v4 engine, as the rsdkv4
  core's notes say), which v5U does not have: in Sonic Origins its own
  program shows the menus.
- **The developer menu reaches play** (the Developer Menu setting on): Dev
  Menu, Stage Select, Regular, the first stage. Green Hill Zone 1, Emerald
  Hill Zone 1 and Palmtree Panic Zone 1 were played running right with
  jumps: the title card, the stage, rings, enemies, the camera following.
  The developer menu also draws the player's position in a corner.
- Sonic CD's mobile pack draws touch controls (a pad and a button) over the
  stage and its title says "TOUCH TO START": it is the mobile release's
  data.
- **Sound.** All three carry their own music and effects, and the core
  plays them: in each 30-second run the sound level (RMS, of 32767) is
  850 to 9000 in each second of the SEGA screen, the title and the stage,
  with silent stretches of a few seconds between (the logos, the
  developer menu, a stage's load). Measured, not listened to.
- **Videos.** Not played. They are files beside Sonic CD's packs
  (videos/*.ogv); the engine looks for Data/Video/<name>.ogv through its
  file layer, and the core gives it only the pack. In a test copy of the
  mobile pack with the four videos added under Data/Video, 4000 frames left
  on the title went to the attract demo, and the engine never asked for a
  video in the first 2400 (a breakpoint on LoadVideo in the native build).
  Whether the core plays a video was not shown.

Measured with Chimera main cb46d2b7 on the converted copies, 1800 frames of
the developer-menu route:

- `run-gate.sh` (RSDKV5U_SONIC1_2013, RSDKV5U_SONIC2_2013,
  RSDKV5U_SONICCD_2011 set): 42 passed, 0 failed, 3 skipped (Sonic
  Origins' packs). For each: the release's pack refused as it comes; the
  copy native = sandbox (picture, sound, frame lengths, lag, memory);
  rerecord = straight (states 15.6, 14.0 and 10.4 MB); a session at frame
  1620 = straight; in a stage, player 1 ran 1819, 2317 and 1566 pixels
  after frame 1000, sound in the last ten seconds. With Sonic CD's PC pack
  in place of the mobile one the new check failed (no stage running, no
  sound), as it should.
- `run-frontend.sh -d <pack> -g <game>`: 10 passed, 0 failed, 1 skipped
  (the test game's globals) for each. The route as a movie made into a
  project by chimera-run and replayed in a new process ends with the
  reference's Game State, Players and picture; the same project opened in
  Chimera's window (headless) too; the stage timer in the saved project
  (00:11.180, 00:11.680, 00:12.200); game.list (135, 173, 144 properties
  with each game's globals).
- The test-game runs, Chimera's contract tests (21 passed, 0 failed),
  check-core-texts.py, check-names.py and check-proper-nouns.py: all pass.

A trap found on the way: chimera-run read a movie with no LogKey by
position, and the Dev Menu column was lost, so the replay stayed on the
title. make-project.py now gives that movie its LogKey.

Not decided, and not changed: whether the core should take these packs
(see "Where this stands").

## The standalone packs stay refused (user-decided, 2026-10-31)

The question the third round put to the owner: should this core take the
packs of the standalone releases (Sonic 1 and Sonic 2 of 2013, Sonic CD of
2011), converting them when the game loads? The owner's answer: no. The
core keeps refusing them, and `waterbox/tests/convert-pack.py` stays a
tool of the tests only, until Sonic Origins' own packs have been given and
tried. Why: the rsdkv4 and rsdkv3 cores already play these packs, with the
releases' own menus; in this core they would start only from the developer
menu, and Sonic CD only from its mobile pack.

What changed: the refusal now tells the user which core plays the pack.

- A pack that starts with "RSDKvB" (the 2013 Sonic 1 and Sonic 2) has its
  own text (`error.pack_is_from_2013_release`): it is a pack of the 2013
  release, this core plays only Sonic Origins' packs, and the rsdkv4 core
  plays this one.
- The 2011 Sonic CD packs have no header, so the core cannot tell one from
  any other file that is not a pack. The text for a file that is not a
  pack (`error.is_not_a_retro_engine_pack`) now goes on: this core plays
  only Sonic Origins' packs, and if the file is a pack of the 2011 Sonic CD
  release, the rsdkv3 core plays it.
- Both texts are in all 26 tables; the new sentences were translated by
  Claude and have not been read by speakers of each language. They keep
  every name whole: where a grammar would put an ending on a core's name,
  the sentence puts the common noun first ("ydin rsdkv4", "Kernen
  rsdkv4"). rsdkv4 and rsdkv3 are now in check-names.py's list of names.

Measured with Chimera main cb46d2b7:

- `run-gate.sh` with the three releases' packs (RSDKV5U_SONIC1_2013,
  RSDKV5U_SONIC2_2013, RSDKV5U_SONICCD_2011): 44 passed, 0 failed, 3
  skipped (Sonic Origins' packs, not given). Two tests are new and run
  without any game: a test game headed "RSDKvB" is refused with the 2013
  text (both builds), and the English texts name rsdkv4 and rsdkv3. The
  releases' packs as they come: Sonic 1 and Sonic 2 refused with the 2013
  text, Sonic CD with the not-a-pack text; their converted copies play as
  before.
- Broken on purpose and seen to fail: with the core looking for "RSDKvX"
  in place of "RSDKvB" the new refusal test failed; with rsdkv4 taken out
  of the Finnish text the names test failed on it (2 failed). Both put
  back, and the gate passed again.
- `run-frontend.sh`: 11 of 11 on the test games; 10 passed, 0 failed, 1
  skipped (the test game's globals) on each of the three releases' packs.
- Chimera's contract tests on a folder holding only the package: 21
  passed, 0 failed. tools/check-core-texts.py (48 keys), check-names.py
  and Chimera's check-proper-nouns.py: all pass.

## A zip of the Data folder in the pack's place (user-decided, 2026-11-03, issue 478)

Issue 478: Sonic Origins' packs give no sound, because Origins keeps its
music and most effects outside them. The reporter unpacked Sonic1u.rsdk,
Sonic2u.rsdk and SonicCDu.rsdk with RetroED, copied the older releases'
Music and SoundFX folders into each Data folder, and ran upstream's
RSDKv5U on the folder ("Sonic 2 (Using Data Folder)", Legacy v4 Mode, in
the issue's picture). They asked the core to take that folder or a zip of
it.

The owner decided (2026-11-03): **take a zipped Data folder.** The game's
data, the firmware of the chosen game, may be the pack or a zip of the
unpacked Data folder, and the sound is whatever the person put in it. Not
chosen: reading Sonic Origins' own sound files.

How (waterbox/rsdk-files.c): a firmware file that starts as a zip
("PK\3\4") is read whole into sealed memory, as a pack is. The Data
folder is the folder in the zip that holds Game/GameConfig.bin nearest the
zip's top, so a zip of the folder ("Data/Game/...") and a zip of its
contents ("Game/...") both work. There is then no Data.rsdk for the
engine, which runs upstream's own "Data Folder" mode and opens its
"Data/..." files one by one; the core finds each in the zip (any case, as
on Windows), unpacks it into the guest's heap with miniz (already in the
build, which reads zips) and frees it when the engine closes it. The
engine version comes from GameConfig.bin as upstream's
DetectEngineVersion reads it when there is no pack. Two new refusals: a
zip with no Game/GameConfig.bin (`error.zip_has_no_data_folder`) and a
file that starts as a zip and cannot be read as one
(`error.zip_not_readable`). The firmware descriptions and the "not a data
pack" refusal now say a zip is taken too; all four texts are in the 26
language tables. Chimera needs no change: these firmware entries pin no
hash, so any file is taken and the project records its hash.

A trap: without a pack, upstream's v3 and v4 modes do not use compiled
scripts (Bytecode/GlobalCode.bin is read only `if (useDataPack && ...)`)
and compile the scripts' text files instead. So a Data folder runs only
if it holds the scripts as text, as the reporter's evidently does. The
core keeps upstream's rule.

Measured (against Chimera 08ee9de1): the core's v4 and v3 test games from
a zip of their Data folder run as from the pack - pictures, sound, frames
and memory the same, native and sandbox; a state saved and loaded before
every frame changes nothing (a state is 5,994,440 bytes against the
pack's 5,982,152); a zip of the folder's contents runs the same.
run-gate.sh 39 passed, 0 failed (6 skipped: the Origins and standalone
packs, not given); run-frontend.sh 12 of 12 (new: engine:data-zip, a
project whose firmware is the zip, through chimera-run); test-packages
all passed. Seen to fail: the core before the change refuses the zip as
"not a Retro Engine data pack". Not tried: Sonic Origins' data, unpacked
or not, which has never been given.

## Where this stands (2026-10-31, fourth round)

The core plays packs in the v5 layout, as Sonic Origins' are, and refuses
the standalone releases' packs, now saying which core plays each (rsdkv4
for the 2013 Sonic 1 and Sonic 2, rsdkv3 for the 2011 Sonic CD). This is
the owner's decision of 2026-10-31 (above); the converter is a test tool
only. Sonic Origins' own packs have still not been given or tried, so
nothing is known yet of the Origins games themselves.

Open, for the owner:

1. Sonic Origins' packs: then `run-gate.sh -1 -2 -c` (or RSDKV5U_SONIC1,
   _SONIC2, _SONICCD) and `run-frontend.sh -d <pack> -g <game>`, the
   developer-menu route. See whether Origins' title leads into a game
   without Origins' program, confirm the file names, add the SHA-1s to
   waterbox.config, and decide the sound (Origins' packs have no music).
   Whether the core should then take the standalone packs too is decided
   after that.
2. The tests on Windows; not run (no Windows runs this round).
3. Publishing, the owner's, after 1.

The third round's hand-over follows, for the record.

## Where this stands (2026-10-31, third round)

The core is unchanged: it plays packs in the v5 layout, as Sonic Origins'
are, and refuses the standalone releases' packs as they come. Sonic
Origins' own packs have still not been given or tried, so nothing is known
yet of the Origins games themselves.

What this round proved, on the owner's 2013 and 2011 packs converted for
the tests (above): the engine, in this core, plays real Sonic 1, Sonic 2
and Sonic CD data in its v4 and v3 modes, with their sound, the same in the
sandbox as natively, with savestates, rerecording, sessions, Chimera's
window and chimera-run. Reached through the developer menu only: the
releases' menus are not in v5U. Sonic CD's PC data and its videos do not
play.

Open, for the owner:

1. Whether this core should take the standalone packs (the question in
   this round's report). Doing it would mean converting them in the core
   at load (the test script's work), and the games would still start only
   from the developer menu, with Sonic CD only from its mobile pack. The
   rsdkv4 and rsdkv3 cores already play these packs with their menus.
2. Sonic Origins' packs: then `run-gate.sh -1 -2 -c` (or RSDKV5U_SONIC1,
   _SONIC2, _SONICCD) and `run-frontend.sh -d <pack> -g <game>`, the
   developer-menu route. See whether Origins' title leads into a game
   without Origins' program, confirm the file names, add the SHA-1s to
   waterbox.config, and decide the sound (Origins' packs have no music).
3. The tests on Windows; not run (no Windows runs this round).
4. Publishing, the owner's, after 2.

The second round's hand-over follows, for the record.

## Where this stands (2026-10-31, second round)

The owner's decisions are in (above): a new core named rsdkv5u, the same
licence terms as the other Retro Engine cores, sound and publishing after
the Origins packs are tried. Every language table keeps names whole, and
the gate checks it.

Works, on the test games only, retested with Chimera main cb46d2b7: the
core in both of the engine's older modes, native and sandbox the same,
savestates, input, lag, saves, properties, settings, the package,
Chimera's window and chimera-run, the contract tests, and a build from a
fresh clone. The gate: 27 passed, 0 failed, 3 skipped (the Origins packs).

Waiting for the owner's Sonic Origins packs. Then:

1. Run `run-gate.sh -1 -2 -c` and `run-frontend.sh -d <pack> -g <game>`,
   look at the pictures (`build/gate/<game>/f*.tga`), and replace the
   guessed routes with real ones (title, menus, into the first zone).
   Confirm the file names and add each pack's SHA-1 to waterbox.config.
2. Check what Origins' scripts ask of the host (the notify callbacks: the
   Origins menu, coins, missions) and whether the games reach play without
   it; check that players 1 and 2 are objects 0 and 1 in Origins' Sonic 1
   and 2.
3. Sound: the owner decides once the packs have been heard.
4. The tests on Windows (the frontend test's scenario on the Windows
   build); not run, this round had no Windows runs.
5. Publishing, the owner's: a public repository, CI
   (`.github/workflows/chimera.yml`, copied from rsdkv5, ids changed, never
   run), a row in Chimera's official-cores.json and README.
