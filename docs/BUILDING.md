# Building and testing the Retro Engine v5U (Sonic Origins) core

## Requirements

Linux, x86-64 (CI uses `ubuntu-latest`). gcc and g++, make, python3, and a
Chimera checkout with miniBox built with its C++ guest toolchain:

```
git clone --recursive https://github.com/ToolAssisted-run/chimera.git <chimera>
mb=<chimera>/extern/chimera-common-minibox
meson setup "$mb/build/meson-linux" "$mb" && meson compile -C "$mb/build/meson-linux"
meson setup "$mb/build/meson-cpp" "$mb" -Dguest_cpp=true && meson compile -C "$mb/build/meson-cpp"
```

For the frontend tests, Chimera built with its window (its README,
"Building"; `tools/ci-chimera.sh` does it). The window runs headless and
needs no display, but it must find Qt's libraries: if they are not where
the system looks, set `LD_LIBRARY_PATH` to Qt's `lib` folder (for example
`$HOME/qt/6.12.0/gcc_64/lib`).

## Sources

- `extern/RSDKv5`: the RSDKv5 decompilation at 191d7ae (its master branch
  on 2026-10-03), with its stb_vorbis submodule. Never edited in place:
  `patches/` is applied by `waterbox/apply-patches.sh`, which every build
  runs first.
- `extern/SDL` (release-2.32.10), `extern/ogg` (v1.3.5), `extern/theora`
  (v1.1.1).

```
git submodule update --init --recursive
```

## Building

miniBox's checkout is given with `MB=` (make), `-m` (scripts) or
`MINIBOX_DIR`; nothing looks for it on its own.

```
cd waterbox
make -f native.mk -j4 MB=<miniBox>   # build/native/run-native, run-wbx
make -f guest.mk -j4 MB=<miniBox>    # build/guest/core.wbx
./build-package.sh -m <miniBox>      # build/package/rsdkv5u.chimeraCore
./build-package.sh -m <miniBox> -o <dir>   # into <dir>
./build-package.sh -r <chimera>      # into <chimera>/build/Cores
```

## Testing

```
./waterbox/run-gate.sh -m <miniBox>                       # the core's tests
./waterbox/run-gate.sh -m <miniBox> -1 <Sonic1u.rsdk> -2 <Sonic2u.rsdk> -c <SonicCDu.rsdk>
./waterbox/tests/run-frontend.sh --chimera-root <chimera> -m <miniBox>
./waterbox/tests/run-frontend.sh --chimera-root <chimera> -m <miniBox> -d <pack> -g sonic1
```

Then Chimera's contract tests against the package, in a folder that holds
only it:

```
<chimera>/build/meson-linux/source/ui/test-packages <folder with the package>
```

The packs may also be given to run-gate.sh as RSDKV5U_SONIC1, RSDKV5U_SONIC2
and RSDKV5U_SONICCD. The standalone releases' packs, which the core refuses
as they come, are tested converted (`waterbox/tests/convert-pack.py`): set
RSDKV5U_SONIC1_2013 and RSDKV5U_SONIC2_2013 to the 2013 releases' data.rsdk
and RSDKV5U_SONICCD_2011 to the 2011 Sonic CD's data_mobile.rsdk, or give
one to run-frontend.sh with `-d`. The real-game tests go through the
engine's developer menu to the first stage (docs/PLAN.md, "The standalone
packs").

Without any pack, both scripts run on the core's two test games
(`waterbox/tests/make-test-game.py`, made at test time): 100 frames of
`waterbox/tests/test.movie` each. The tests on the real packs say SKIP
without them. Keep the packs outside the repository, or in
`tests/roms-local/`, which git ignores.

## What CI does

`.github/workflows/chimera.yml`: builds Chimera and miniBox, runs
`run-gate.sh` and `run-frontend.sh` (the test games), builds the package
stamped with the commit, runs the contract tests, and publishes through
Chimera's `publish-core.yml`. It has not been run: the repository is local.

## Troubleshooting

- "not a Retro Engine data pack": the file does not start with RSDKv3,
  RSDKv4 or RSDKv5.
- "a Retro Engine v5 data pack": a v5 game (Sonic Mania, Sonic 3 &
  Knuckles) needs its logic as a program; this core plays v3 and v4 games.
- An edit to an engine .cpp that another one includes does not rebuild:
  touch the including file (for the legacy code, RetroEngine.cpp, Scene.cpp,
  Object.cpp and the others listed by `grep -rn '#include ".*\.cpp"'`).
- Native and sandbox differ: a math function the engine or a codec calls is
  not wrapped (sources.mk MATH_FUNCS).
