# Retro Engine v5U (Sonic Origins) for Chimera

This repository builds the RSDKv5 decompilation, a decompilation of the
Retro Engine v5 and v5U ("Ultimate", the engine of Sonic Origins), as a game
core for [Chimera](https://github.com/ToolAssisted-run/chimera), a frontend
for tool-assisted speedruns. It plays the older games Sonic Origins carries -
Sonic 1, Sonic 2 and Sonic CD - from Sonic Origins' own data packs, frame by
frame, the same every time, with savestates, rewinding and movies.

Sonic 3 & Knuckles is not played: its game logic is a program inside Sonic
Origins' executable, which no decompilation provides.

The build makes one file, `rsdkv5u.chimeraCore`. Put it in Chimera's
`Cores` folder.

## What you bring

The game's data pack from a Sonic Origins installation, as the project's
firmware: Sonic1u.rsdk, Sonic2u.rsdk or SonicCDu.rsdk, the Game setting
choosing which. In its place you can give a zip of the game's Data folder,
unpacked from the pack (RetroED unpacks it), with anything you add to it,
such as an older release's Music and SoundFX folders for the sound. This
package contains none of the games. No pack of Sonic Origins has been tried
yet (docs/PLAN.md).

## Settings

Game, language, screen width (320 to 424 pixels, 424 by default), Plus
content (off by default), the random seed, and the engine's developer menu
(with a Dev Menu control to open it). Each is recorded in the project.

## What it does not do yet

- Sonic Origins' own packs have not been tried; the tests run on two small
  games written for them.
- The standalone releases' packs are refused, and the refusal names the
  core that plays them: rsdkv4 for Sonic 1 and Sonic 2 (2013), rsdkv3 for
  Sonic CD (2011).
- Sound: Sonic Origins keeps most of its music and effects outside these
  packs. A zip of the Data folder plays whatever music and effects you put
  in it; the core reads no sound files of Sonic Origins' own.
- Mods: the decompilation's mod loader is off.

[docs/PLAN.md](docs/PLAN.md) says what was decided and measured,
[docs/BUILDING.md](docs/BUILDING.md) how to build and test it.

## Licence

This repository's own files are MIT. The engine is the RSDKv5 decompilation
(by Rubberduckycooly and chuliRMG, of the Retro Engine v5 and v5U by Evening
Star), under its own licence in [LICENSE-RSDK.txt](LICENSE-RSDK.txt): no use
for profit, credit to its authors, downloadable content off by default, no
game assets distributed. A package built from here is under those terms.
[LICENSE](LICENSE) says what each part is under.
