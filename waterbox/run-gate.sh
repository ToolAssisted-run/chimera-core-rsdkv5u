#!/bin/sh
# run-gate.sh - the Retro Engine v5U (Sonic Origins) core's tests: the native
# reference and core.wbx are the same machine, on the core's two test games
# (tests/make-test-game.py: one for the engine's v4 mode, as Sonic 1 and 2
# are, one for its v3 mode, as Sonic CD is) and, when they are given, on
# Sonic Origins' own data packs.
#
# Every test says what it compared. The tests, on each test game:
#   equivalence  run-native and run-wbx: every frame's picture, sound, length
#                and lag, the frame count and every memory domain
#   rerecord     the sandbox saved and loaded before every frame = without
#   session      saved at a frame, a new host, loaded, finished = without
#   input        the movie's presses reach the game's script: four presses of
#                A counted; the frame that loads the stage is the only lag
#                frame
#   saves        the game's save written (Start) and read back (C) in the same
#                run, in both builds, and across a session started after the
#                write
# and once:
#   turbo        the first half not drawn: the rest the same, and the first
#                half's pictures really not drawn
#   pokes        a property written before a frame lands in the game: a
#                player's position, a global variable by its name
#   settings     randomSeed is what the game's random numbers follow;
#                screenWidth is the picture's width; the game setting picks
#                the firmware file; the Plus setting reaches the game
#                (game.hasPlusDLC)
#   refusals     no data file, a file that is not a pack, a v5 pack (its game
#                is a program this core does not carry), a screen width out
#                of range: each says why
#   clock        the guest has no time() of its own: the one it calls is the
#                core's (the setting)
#   teeth        the equivalence comparison sees a one-frame difference
#   firmware     waterbox.config's firmware is the files the core opens, one a
#                game
#   names        no name (the core's, the engine's, a game's, a character's)
#                is translated or has an ending put on it in any of the 26
#                language tables (tests/check-names.py)
#   origins      (with -1, -2, -c) the equivalence, rerecord and session
#                tests on Sonic Origins' Sonic 1, Sonic 2 and Sonic CD data
#                packs, which are never in the repository
#
# usage: run-gate.sh -m <miniBox dir> [-1 <Sonic1u.rsdk>] [-2 <Sonic2u.rsdk>] [-c <SonicCDu.rsdk>]
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
mb="${MINIBOX_DIR:-}"
sonic1=""
sonic2=""
soniccd=""
while getopts "m:1:2:c:" opt; do
	case "$opt" in
		m) mb="$OPTARG" ;;
		1) sonic1="$OPTARG" ;;
		2) sonic2="$OPTARG" ;;
		c) soniccd="$OPTARG" ;;
		*) exit 2 ;;
	esac
done
[ -n "$mb" ] || { echo "give miniBox's checkout: -m <chimera>/extern/chimera-common-minibox (or MINIBOX_DIR)"; exit 2; }
mb="$(cd "$mb" && pwd)"
jobs="${JOBS:-4}"

echo "== build (miniBox $mb)"
mkdir -p "$root/build"
make -C "$here" -f native.mk MB="$mb" -j"$jobs" > "$root/build/gate-native.log" 2>&1 || {
	tail -20 "$root/build/gate-native.log"; echo "native build failed"; exit 1; }
make -C "$here" -f guest.mk MB="$mb" -j"$jobs" > "$root/build/gate-guest.log" 2>&1 || {
	tail -20 "$root/build/gate-guest.log"; echo "guest build failed"; exit 1; }
native="$root/build/native/run-native"
wbx="$root/build/native/run-wbx"
core="$root/build/guest/core.wbx"
movie="$here/tests/test.movie"
frames=100

work="$root/build/gate"
rm -rf "$work"
mkdir -p "$work"
fails=0
pass() { echo "PASS $*"; }
fail() { echo "FAIL $*"; fails=$((fails + 1)); }

# the digests only (miniBox's own log lines go to the same stdout)
digests() { grep -E '^[A-Za-z]+(\[[^]]*\])?=' "$1" || true; }
same() { # same <test> <a> <b> <what>
	digests "$2" > "$2.d"
	digests "$3" > "$3.d"
	if cmp -s "$2.d" "$3.d"; then
		pass "$1: $4"
	else
		fail "$1: $4"; diff "$2.d" "$3.d" | head -20
	fi
}
value() { digests "$1" | sed -n "s/^$2=//p"; }
# a traced property's value at a frame (a trace line: frame rate read values...)
at() { awk -v f="$2" -v col="$3" '$1 == f { print $(3 + col) }' "$1"; }
size() { python3 -c "import sys; d = open(sys.argv[1], 'rb').read(18); print('%dx%d' % (d[12] | d[13] << 8, d[14] | d[15] << 8))" "$1" 2>/dev/null || echo none; }

# mkgame <dir> <v3|v4> [<settings JSON>]: a test game as the firmware file of
# the game the settings choose (v4: Sonic 1's, v3: Sonic CD's, by default)
mkgame() {
	mkdir -p "$work/$1"
	if [ "$2" = v3 ]; then def='{"game": "soniccd"}'; file=SonicCDu.rsdk; else def='{"game": "sonic1"}'; file=Sonic1u.rsdk; fi
	settings="${3:-$def}"
	case "$settings" in *'"sonic2"'*) file=Sonic2u.rsdk ;; *'"soniccd"'*) file=SonicCDu.rsdk ;; *'"sonic1"'*) file=Sonic1u.rsdk ;; esac
	python3 "$here/tests/make-test-game.py" "$2" "$work/$1/$file"
	printf '%s' "$settings" > "$work/$1/settings"
}

props="Global.test.presses,Global.test.random,Global.test.saved"
for v in v4 v3; do
	mkgame "$v" "$v"
	echo "== the $v test game ($frames frames, $movie)"
	"$native" "$work/$v" --frames $frames --movie "$movie" > "$work/$v.n"
	"$wbx" "$core" "$work/$v" --frames $frames --movie "$movie" > "$work/$v.w" 2> "$work/$v.werr"
	same "$v equivalence" "$work/$v.n" "$work/$v.w" "native = sandbox, $frames frames (lag $(value "$work/$v.n" lagFrames))"
	"$wbx" "$core" "$work/$v" --frames $frames --movie "$movie" --rerecord > "$work/$v.r" 2> "$work/$v.rerr"
	same "$v rerecord" "$work/$v.r" "$work/$v.w" "save+load before every frame = straight ($(sed -n 's/stateBytes=//p' "$work/$v.rerr") state bytes)"
	"$wbx" "$core" "$work/$v" --frames $frames --movie "$movie" --session-at 75 > "$work/$v.s" 2> "$work/$v.serr"
	same "$v session" "$work/$v.s" "$work/$v.w" "saved at frame 75 (after the game's save was written), loaded into a new host = straight"

	"$native" "$work/$v" --frames $frames --movie "$movie" --trace "$work/$v.in.n" --trace-props "$props" > /dev/null
	"$wbx" "$core" "$work/$v" --frames $frames --movie "$movie" --trace "$work/$v.in.w" --trace-props "$props" > /dev/null 2>&1
	"$wbx" "$core" "$work/$v" --frames $frames --movie "$movie" --session-at 75 --trace "$work/$v.in.s" --trace-props "$props" > /dev/null 2>&1
	# A at frames 10, 40, 60 and 80 (the movie's lines)
	if [ "$(at "$work/$v.in.n" 9 1)" = 0 ] && [ "$(at "$work/$v.in.n" 10 1)" = 1 ] && [ "$(at "$work/$v.in.n" 79 1)" = 3 ] &&
	   [ "$(at "$work/$v.in.n" 99 1)" = 4 ] && [ "$(value "$work/$v.n" lagFrames)" = 1 ] && [ "$(at "$work/$v.in.n" 0 0)" = 0 ]; then
		pass "$v input: A counted at frame 10, 3 times by frame 79 and 4 by 99; one lag frame, the stage's load (frame 0)"
	else
		fail "$v input: presses $(at "$work/$v.in.n" 9 1)/$(at "$work/$v.in.n" 10 1)/$(at "$work/$v.in.n" 79 1)/$(at "$work/$v.in.n" 99 1), lag $(value "$work/$v.n" lagFrames), frame 0 read $(at "$work/$v.in.n" 0 0)"
	fi
	if [ "$(at "$work/$v.in.n" 89 3)" = -1 ] && [ "$(at "$work/$v.in.n" 90 3)" = 3 ] && cmp -s "$work/$v.in.n" "$work/$v.in.w"; then
		pass "$v saves: the count written at frame 70 (3) is read back at frame 90, in both builds"
	else
		fail "$v saves: before the read $(at "$work/$v.in.n" 89 3), after $(at "$work/$v.in.n" 90 3)"
	fi
	if [ "$(at "$work/$v.in.s" 90 3)" = 3 ]; then
		pass "$v saves: a session started at frame 75 reads back the save written at 70"
	else
		fail "$v saves: after a session, read back $(at "$work/$v.in.s" 90 3)"
	fi
done

echo "== turbo"
"$native" "$work/v4" --frames $frames --movie "$movie" --turbo > "$work/t.txt"
digests "$work/t.txt" | grep -v '^videoHash=' > "$work/t.d"
digests "$work/v4.n" | grep -v '^videoHash=' > "$work/n.d"
if cmp -s "$work/t.d" "$work/n.d" && [ "$(value "$work/t.txt" videoHash)" != "$(value "$work/v4.n" videoHash)" ]; then
	pass "turbo: first half undrawn - the second half's pictures, the sound, the frames and memory the same; the first half's pictures not"
else
	fail "turbo"; diff "$work/t.d" "$work/n.d" | head
fi

echo "== pokes"
for v in v4 v3; do
	"$native" "$work/$v" --frames 30 --poke "20:Player 1.X=13107200" --poke "25:Global.test.presses=40" --trace "$work/$v.pk.n" --trace-props "Player 1.X,Global.test.presses" > /dev/null
	"$wbx" "$core" "$work/$v" --frames 30 --poke "20:Player 1.X=13107200" --poke "25:Global.test.presses=40" --trace "$work/$v.pk.w" --trace-props "Player 1.X,Global.test.presses" > /dev/null 2>&1
	if [ "$(at "$work/$v.pk.n" 19 1)" = 0 ] && [ "$(at "$work/$v.pk.n" 29 1)" = 13107200 ] && [ "$(at "$work/$v.pk.n" 24 2)" = 0 ] &&
	   [ "$(at "$work/$v.pk.n" 29 2)" = 40 ] && cmp -s "$work/$v.pk.n" "$work/$v.pk.w"; then
		pass "$v pokes: Player 1.X and Global.test.presses (a global variable by its name) written before a frame are what the game has after it, in both builds"
	else
		fail "$v pokes: x $(at "$work/$v.pk.n" 19 1)/$(at "$work/$v.pk.n" 29 1), presses $(at "$work/$v.pk.n" 24 2)/$(at "$work/$v.pk.n" 29 2)"
	fi
done

echo "== settings"
seeded() { # seeded <dir> <seed>
	mkgame "$1" v4 "{\"game\": \"sonic1\", \"randomSeed\": $2}"
	"$native" "$work/$1" --frames $frames --movie "$movie" --trace "$work/$1/t" --trace-props Global.test.random > /dev/null
	"$wbx" "$core" "$work/$1" --frames $frames --movie "$movie" --trace "$work/$1/tw" --trace-props Global.test.random > /dev/null 2>&1
	awk 'NR > 1 { print $4 }' "$work/$1/t" | uniq | tr '\n' ' '
}
s0="$(seeded seed0 0)"
s0b="$(seeded seed0b 0)"
s7="$(seeded seed7 7)"
if [ "$s0" = "$s0b" ] && [ "$s0" != "$s7" ] && cmp -s "$work/seed7/t" "$work/seed7/tw"; then
	pass "settings: randomSeed 0 draws ${s0% } twice, 7 draws ${s7% }; both builds the same"
else
	fail "settings: randomSeed 0 ${s0}/${s0b}, 7 ${s7}"
fi
mkgame narrow v4 '{"game": "sonic1", "screenWidth": 320}'
"$native" "$work/narrow" --frames 10 --screenshot 9:"$work/narrow/s.tga" > "$work/narrow/n" 2>&1 || true
"$wbx" "$core" "$work/narrow" --frames 10 > "$work/narrow/w" 2>&1 || true
if [ "$(size "$work/narrow/s.tga")" = 320x240 ]; then
	same settings "$work/narrow/w" "$work/narrow/n" "screenWidth 320 draws a 320x240 picture; native = sandbox"
else
	fail "settings: screenWidth 320 drew $(size "$work/narrow/s.tga")"
fi
mkgame second v4 '{"game": "sonic2"}'
"$native" "$work/second" --frames $frames --movie "$movie" > "$work/second/n" 2>&1 || true
if digests "$work/second/n" | cmp -s - "$work/v4.n.d"; then
	pass "settings: the game setting sonic2 plays Sonic2u.rsdk (the same test game, the same run)"
else
	fail "settings: game sonic2"; tail -2 "$work/second/n"
fi
# the Origins Plus content: the scripts read it as the global game.hasPlusDLC,
# which the engine sets when it reads the game's configuration (patches/0003)
mkgame plus v4 '{"game": "sonic1", "plus": true}'
"$native" "$work/v4" --frames 2 --trace "$work/plus/off" --trace-props Global.game.hasPlusDLC > "$work/plus/off.n" 2>&1 || true
"$native" "$work/plus" --frames 2 --trace "$work/plus/on" --trace-props Global.game.hasPlusDLC > "$work/plus/on.n" 2>&1 || true
"$wbx" "$core" "$work/plus" --frames 2 --trace "$work/plus/onw" --trace-props Global.game.hasPlusDLC > /dev/null 2>&1 || true
if [ "$(at "$work/plus/off" 1 1)" = 0 ] && [ "$(at "$work/plus/on" 1 1)" = 1 ] && cmp -s "$work/plus/on" "$work/plus/onw"; then
	pass "settings: plus off (the default) leaves game.hasPlusDLC 0, on sets it 1; both builds the same"
else
	fail "settings: plus - game.hasPlusDLC off $(at "$work/plus/off" 1 1), on $(at "$work/plus/on" 1 1)"
fi

echo "== refusals"
refuse() { # refuse <dir> <expected text> <what>
	out="$("$native" "$work/$1" --frames 1 2>&1 || true)"
	outw="$("$wbx" "$core" "$work/$1" --frames 1 2>&1 || true)"
	if echo "$out" | grep -q "loadError=.*$2" && echo "$outw" | grep -q "loadError=.*$2"; then
		pass "refusals: $3 - $(echo "$out" | sed -n 's/^loadError=//p' | tr '\037' ' ')"
	else
		fail "refusals: $3"; echo "$out" | tail -2
	fi
}
mkdir -p "$work/none" "$work/junk" "$work/v5"
printf '{"game": "sonic1"}' > "$work/none/settings"
head -c 100000 /dev/zero > "$work/junk/Sonic1u.rsdk"
printf '{"game": "sonic1"}' > "$work/junk/settings"
python3 "$here/tests/make-test-game.py" v4 "$work/v5/Sonic1u.rsdk"
printf 'RSDKv5' | dd of="$work/v5/Sonic1u.rsdk" conv=notrunc 2>/dev/null
printf '{"game": "sonic1"}' > "$work/v5/settings"
mkgame wide v4 '{"game": "sonic1", "screenWidth": 640}'
refuse none "error.game_s_data_file_missing.Sonic1u.rsdk" "no data file"
refuse junk "error.is_not_a_retro_engine_pack" "a file of zeros"
refuse v5 "error.pack_is_not_v3_or_v4.Sonic1u.rsdk.5" "a v5 pack"
refuse wide "error.setting_screenwidth_is_it" "screenWidth 640"

echo "== clock"
if nm "$core" | grep -qE ' (T|W|t) (time|__real_time)$'; then
	fail "clock: the guest links a time() of the C library's"
else
	pass "clock: time() is only the core's (the randomSeed setting); SDL's counters are the frame count"
fi

echo "== teeth"
# frame 30 is in the Right stretch: Left instead, which the test game moves by
awk '!/^#/ { n++ } n == 31 && !/^#/ { print "L"; next } { print }' "$movie" > "$work/teeth.movie"
"$native" "$work/v4" --frames $frames --movie "$work/teeth.movie" > "$work/teeth.txt"
if digests "$work/teeth.txt" | cmp -s - "$work/v4.n.d"; then
	fail "teeth: a movie one frame different digests the same"
else
	pass "teeth: a movie one frame different digests differently"
fi

echo "== firmware"
fw="$(python3 -c "import json, sys; print(' '.join(f['id'] + '/' + f['requiredWhen']['is'] for f in json.load(open(sys.argv[1]))['firmware']))" "$here/waterbox.config")"
if [ "$fw" = "Sonic1u.rsdk/sonic1 Sonic2u.rsdk/sonic2 SonicCDu.rsdk/soniccd" ] &&
   grep -q 'k_firmware\[RSDK_GAME_COUNT\] = { "Sonic1u.rsdk", "Sonic2u.rsdk", "SonicCDu.rsdk" }' "$here/rsdk-files.c"; then
	pass "firmware: waterbox.config's three firmware files, one a game, are the names the core opens"
else
	fail "firmware: waterbox.config declares '$fw'"
fi

# names are never translated or bent (Chimera's owner, 2026-10-31)
if python3 "$here/tests/check-names.py" "$here/languages" > "$work/names.txt" 2>&1; then
	pass "names: $(tail -1 "$work/names.txt")"
else
	fail "names: a name is translated or bent in a language table"; head -20 "$work/names.txt"
fi

# Sonic Origins' own packs. The route is a guess made without the data: Start
# and A now and then through the title and the menus, then right and jumps.
route="--press 600:S:5 --press 900:S:5 --press 1200:A:5 --press 1500:S:5 --press 1800:A:5 \
	--press 2100:R:900 --press 2400:A:10 --press 2700:A:10"
origins() { # origins <name> <setting> <firmware name> <file>
	[ -n "$4" ] || { echo "SKIP $1: no data pack given; it is never in the repository"; return; }
	echo "== $1 ($4)"
	d="$work/$2"
	mkdir -p "$d"
	cp "$4" "$d/$3"
	printf '{"game": "%s"}' "$2" > "$d/settings"
	gn=3000
	gat=$((gn * 9 / 10))
	"$native" "$d" --frames $gn $route --trace "$d/t" --trace-props "Engine.Version,Stage.Category,Stage.List Position" \
		--screenshot 300:"$d/f300.tga" --screenshot 1000:"$d/f1000.tga" --screenshot 2900:"$d/f2900.tga" > "$d/n"
	"$wbx" "$core" "$d" --frames $gn $route > "$d/w" 2>/dev/null
	stage="$(tail -1 "$d/t" | awk '{ print "v" $4 ", list " $5 "/" $6 }')"
	same "$1" "$d/w" "$d/n" "native = sandbox, $gn frames (at the end: $stage)"
	"$wbx" "$core" "$d" --frames $gn $route --rerecord > "$d/r" 2>/dev/null
	same "$1" "$d/r" "$d/w" "rerecord = straight"
	"$wbx" "$core" "$d" --frames $gn $route --session-at $gat > "$d/s" 2>/dev/null
	same "$1" "$d/s" "$d/w" "session at $gat = straight"
}
origins "Sonic 1" sonic1 Sonic1u.rsdk "$sonic1"
origins "Sonic 2" sonic2 Sonic2u.rsdk "$sonic2"
origins "Sonic CD" soniccd SonicCDu.rsdk "$soniccd"

echo
if [ $fails -eq 0 ]; then
	echo "gate: all tests passed"
else
	echo "gate: $fails test(s) failed"
	exit 1
fi
