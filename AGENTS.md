# AGENTS.md - Retro Engine v5U (Sonic Origins) core for Chimera

This repository builds the RSDKv5 decompilation as v5U, the engine of Sonic
Origins, as a core for Chimera (https://github.com/ToolAssisted-run/chimera).
It plays Sonic Origins' Sonic 1 and Sonic 2 (the engine's v4 mode) and Sonic
CD (its v3 mode) from their data packs; no game logic is compiled in. It is
a GAME core (`"kind": "game"`). It produces one file, `rsdkv5u.chimeraCore`.
The games' data packs are never in the repository or the package: the user
provides them.

## Layout

- `extern/RSDKv5`: the engine's decompilation, a submodule. Never edited in
  place. `extern/{SDL,ogg,theora}`: library submodules.
- `patches/`: numbered patches for `extern/RSDKv5`, applied by
  `waterbox/apply-patches.sh`.
- `waterbox/sources.mk`: the sources, defines and link wraps both builds
  share. `native.mk`, `guest.mk`: the two builds.
- `waterbox/rsdk-driver.cpp`: the machine (a frame, the sound, the picture).
  `rsdk-files.c`: every file the engine opens, and the Game setting's
  firmware. `seams.c`: what SDL is answered with. `detmath.c`: the math
  functions. `game-state.cpp`: the memory domains and properties.
  `wbx-entry.c`: the guest's exports.
- `waterbox/waterbox.config`, `file_slots.json`, `default_keybinds.json`,
  `package-licenses.json`, `languages/`: the declarations and texts.
- `waterbox/run-gate.sh`: the core's tests. `waterbox/tests/`: the frontend
  tests and `make-test-game.py`, the two test games (v4 and v3).
- `build/`: every output. `tests/roms-local/`: game data, ignored by git.

## Commands, in order

```
git submodule update --init --recursive
cd waterbox
./run-gate.sh -m <miniBox> [-1 <Sonic1u.rsdk>] [-2 <Sonic2u.rsdk>] [-c <SonicCDu.rsdk>]
./tests/run-frontend.sh --chimera-root <chimera> -m <miniBox> [-d <pack> -g sonic1|sonic2|soniccd]
./build-package.sh -m <miniBox> -o <out dir>
```

`docs/BUILDING.md` has the requirements and the contract tests. Builds use
`JOBS` (default 4) for make.

## Rules

- Upstream is changed only by a numbered patch. The decompilation's licence
  (LICENSE-RSDK.txt) covers the patches: keep it in the repository and the
  package, keep `RSDK_AUTOBUILD` defined, the Plus setting off by default,
  and distribute no game asset.
- Native and sandbox must stay the same machine: anything that reads a
  clock, a thread, the C library's math or rand is answered by the core.
- Every new test is broken on purpose once and seen to fail.
- An edit to an engine file that another .cpp includes (the legacy code)
  does not rebuild by itself: touch the including file.
- No game data in the repository, in CI or in an issue.
- Names are never translated or bent in the language tables (Chimera's
  owner, 2026-10-31): a grammar that puts endings on names gets a common
  noun beside the name instead. `waterbox/tests/check-names.py` checks it,
  in the gate; add a new name to its list.
- Plain language and ASCII in documents. Commits: `type(rsdkv5u): what`, a
  short body with the numbers, explicit paths staged.
