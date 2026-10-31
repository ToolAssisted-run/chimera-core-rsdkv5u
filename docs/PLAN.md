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

## Where this stands (2026-10-31)

Works, on the test games only: the core in both legacy modes, native and
sandbox the same, savestates, input, lag, saves, properties, settings, the
package, Chimera's window and chimera-run, the contract tests.

Next:

1. With the owner's Sonic Origins packs: run `run-gate.sh -1 -2 -c` and
   `run-frontend.sh -d <pack> -g <game>`, look at the pictures
   (`build/gate/<game>/f*.tga`), and replace the guessed routes with real
   ones (title, menus, into the first zone). Confirm the file names and add
   each pack's SHA-1 to waterbox.config.
2. Check what Origins' scripts ask of the host (the notify callbacks: the
   Origins menu, coins, missions) and whether the games reach play without
   it; check that Player 1 and 2 are objects 0 and 1 in Origins' Sonic 1/2.
3. Sound, if the packs are silent (question above).
4. The tests on Windows (the frontend test's scenario on the Windows build).
5. If the owner chooses a new public repository: CI is in
   `.github/workflows/chimera.yml` (copied from rsdkv5, ids changed, not run).
