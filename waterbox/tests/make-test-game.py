#!/usr/bin/env python3
"""Two small Retro Engine games made for the core's tests, written out from
scratch: no part of any Sega game is in them.

    make-test-game.py v4 <out.rsdk>     a game for the engine's v4 mode
    make-test-game.py v3 <out.rsdk>     a game for the engine's v3 mode
    make-test-game.py v4|v3 <out.zip>   the same game's files as a zip of
                                        its Data folder (issue 478), each
                                        compressed, as a person zips the
                                        folder a pack was unpacked into

Sonic Origins keeps Sonic 1 and Sonic 2 as v4 games and Sonic CD as a v3
game, each in a data pack of the v5 engine's layout whose header names the
older engine ("RSDKv4", "RSDKv3"): RSDKv5 Reader.cpp DetectEngineVersion
reads that character to choose the mode. These test packs have the same
shape: "RSDK", "v", the version character, the number of files, and for
each file the MD5 of its lower-case path (four 32-bit words, each read
most significant byte first), its offset and its size. Every file is
stored unencrypted, which the format allows (the top bit of the size).

What is in a pack:

  Data/Game/GameConfig.bin  the game: four global variables (test.presses,
                            test.random, test.saved, game.hasPlusDLC), one global sound
                            effect, one stage in the presentation list
  Data/SoundFX/Test/Beep.wav  a short square wave
  Data/Music/Tune.ogg       tests/tune.ogg, two seconds of two tones
  Data/Stages/Test/StageConfig.bin, Act1.bin  the stage: one object
  Data/Scripts/Test/Box.txt the object's script, in the engine's script
                            language of that version, which the engine
                            compiles at load (there is no bytecode)
  (v3 only) Data/Palettes/MasterPalette.act

The object, Box, is the test: it starts the stage's timer and the music,
moves with Left and Right, and on each press of A plays the beep, counts
the press in test.presses and draws a random number into test.random (the
engine's Rand). Start saves the count in the game's save (saveRAM[0],
WriteSaveRAM); C reads the save back (ReadSaveRAM) into test.saved. It
draws itself as a rectangle whose colour changes with the presses, and a
second rectangle as wide as the random number, so each shows in the
picture.

The game files are the ones the chimera-core-rsdkv3 and chimera-core-rsdkv4
test games carry (their make-test-game.py), but for two things: the pack is
the v5 layout, and the scripts read the controls as keyDown[0] and
keyPress[0]. In the v5 engine's legacy modes a control read without an
index reads the controller numbered as the object reading it (RSDKv5
ScriptLegacyv4.cpp: VARARR_NONE is objectEntityPos), which is right for a
game's players - objects 0 and 1 - and wrong for this object, which the
stage puts further on; 0 is any controller.
"""
import hashlib
import os
import struct
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))


def pack(version, files):
    """files: {"Data/Dir/Name": bytes} -> the pack's bytes"""
    names = sorted(files)
    header = 6 + 2 + 24 * len(names)
    out = bytearray(b"RSDKv" + version.encode() + struct.pack("<H", len(names)))
    body = bytearray()
    for name in names:
        dg = hashlib.md5(name.lower().encode()).digest()
        out += b"".join(dg[4 * i:4 * i + 4][::-1] for i in range(4))
        out += struct.pack("<II", header + len(body), len(files[name]))
        body += files[name]
    return bytes(out + body)


def s8(text):
    b = text.encode()
    return bytes([len(b)]) + b


def beep():
    rate, n = 44100, 4410
    samples = b"".join(struct.pack("<h", 6000 if (i // 50) % 2 else -6000) for i in range(n))
    fmt = struct.pack("<HHIIHH", 1, 1, rate, rate * 2, 2, 16)
    data = b"RIFF" + struct.pack("<I", 36 + len(samples)) + b"WAVE"
    data += b"fmt " + struct.pack("<I", 16) + fmt + b"data" + struct.pack("<I", len(samples)) + samples
    return data


# game.hasPlusDLC is the global the engine sets from the Plus setting when it
# reads the configuration (RetroEnginev4.cpp and v3, patches/0003), as Sonic
# Origins' own games declare it
VARIABLES = [("test.presses", 0), ("test.random", 0), ("test.saved", -1), ("game.hasPlusDLC", 0)]

# ------------------------------------------------------------------ v4


def v4_game_config():
    out = s8("Chimera Test") + s8("A game made for the core's tests")
    for i in range(0x60):                               # the global palette
        out += bytes([(i * 37) & 0xFF, (i * 91) & 0xFF, (i * 53) & 0xFF])
    out += bytes([0])                                   # no global objects
    out += bytes([len(VARIABLES)])
    for name, value in VARIABLES:
        out += s8(name) + struct.pack(">i", value)
    out += bytes([1]) + s8("Beep") + s8("Test/Beep.wav")   # global sound effects: names, then paths
    out += bytes([0])                                   # players
    # the four stage lists in the file's order: presentation, regular,
    # special, bonus; each stage: folder, act id, name, highlighted
    out += bytes([1]) + s8("Test") + s8("1") + s8("TEST STAGE") + bytes([0])
    out += bytes([0]) + bytes([0]) + bytes([0])
    return out


def v4_stage_config():
    out = bytes([0])                                    # no global scripts
    for i in range(32):                                 # palette 0x60..0x7F
        out += bytes([i * 8, 255 - i * 8, 128])
    out += bytes([0])                                   # no stage sound effects
    out += bytes([1]) + s8("Box")                       # one object type
    out += s8("Test/Box.txt")                           # its script
    return out


def v4_act():
    out = s8("TEST")
    out += bytes([9, 9, 9, 9]) + bytes([3])             # no tile layers drawn
    out += bytes([1, 0, 1, 0]) + b"\x00\x00"            # a 1x1 layout, empty
    out += struct.pack("<H", 1)                         # one object
    out += struct.pack("<HBBii", 0, 1, 0, 100 << 16, 120 << 16)  # Box at (100, 120)
    return out


V4_BOX = """// Box: the core's test object
private alias object.value0 : object.frames
private alias object.value1 : object.colour

event ObjectUpdate
	object.frames++
	if keyDown[0].left == true
		object.xpos -= 0x20000
	end if
	if keyDown[0].right == true
		object.xpos += 0x20000
	end if
	if keyPress[0].buttonA == true
		PlaySfx(0, 0)
		test.presses++
		Rand(temp0, 200)
		test.random = temp0
		object.colour += 40
	end if
	if keyPress[0].start == true
		saveRAM[0] = test.presses
		WriteSaveRAM()
	end if
	if keyPress[0].buttonC == true
		saveRAM[0] = 0
		ReadSaveRAM()
		test.saved = saveRAM[0]
	end if
end event

event ObjectDraw
	ClearScreen(0)
	temp1 = object.colour
	temp1 &= 255
	DrawRect(object.ixpos, object.iypos, 16, 16, 255, temp1, 0, 255)
	DrawRect(8, 8, test.random, 4, 255, 255, 255, 255)
end event

event ObjectStartup
	stage.timeEnabled = true
	SetMusicTrack("Tune.ogg", 0, 1)
	PlayMusic(0)
end event
"""

# ------------------------------------------------------------------ v3


def v3_game_config():
    out = s8("Chimera Test") + s8("Data") + s8("A game made for the core's tests")
    out += bytes([0])                                   # no global objects
    out += bytes([len(VARIABLES)])
    for name, value in VARIABLES:
        out += s8(name) + struct.pack(">i", value)
    out += bytes([1]) + s8("Test/Beep.wav")             # global sound effects
    out += bytes([0])                                   # players
    out += bytes([1]) + s8("Test") + s8("1") + s8("TEST STAGE") + bytes([0])
    out += bytes([0]) + bytes([0]) + bytes([0])
    return out


def v3_palette():
    out = bytearray()
    for i in range(256):
        out += bytes([(i * 37) & 0xFF, (i * 91) & 0xFF, (i * 53) & 0xFF])
    out[0:3] = b"\x10\x20\x40"
    return bytes(out)


def v3_stage_config():
    out = bytes([0])                                    # no global scripts
    for i in range(32):                                 # palette 96..127
        out += bytes([i * 8, 255 - i * 8, 128])
    out += bytes([1]) + s8("Box")                       # one object type
    out += s8("Test/Box.txt")                           # its script
    out += bytes([0])                                   # no stage sound effects
    return out


def v3_act():
    out = s8("TEST")
    out += bytes([9, 9, 9, 9]) + bytes([3])             # no tile layers drawn
    out += bytes([1, 1]) + b"\x00\x00"                  # a 1x1 layout, empty
    out += bytes([0])                                   # no type names
    out += struct.pack(">H", 1)                         # one object
    out += bytes([1, 0]) + struct.pack(">HH", 100, 120) # Box at (100, 120)
    return out


V3_BOX = """// Box: the core's test object
#alias Object.Value0 : Box.Frames
#alias Object.Value1 : Box.Colour

sub ObjectMain
	Box.Frames++
	if KeyDown[0].Left == 1
		Object.XPos -= 0x20000
	end if
	if KeyDown[0].Right == 1
		Object.XPos += 0x20000
	end if
	if KeyPress[0].ButtonA == 1
		PlaySfx(0, 0)
		test.presses++
		Rand(TempValue0, 200)
		test.random = TempValue0
		Box.Colour += 40
	end if
	if KeyPress[0].Start == 1
		SaveRAM[0] = test.presses
		WriteSaveRAM()
	end if
	if KeyPress[0].ButtonC == 1
		SaveRAM[0] = 0
		ReadSaveRAM()
		test.saved = SaveRAM[0]
	end if
end sub

sub ObjectDraw
	ClearScreen(0)
	TempValue1 = Box.Colour
	TempValue1 &= 255
	DrawRect(Object.iXPos, Object.iYPos, 16, 16, 255, TempValue1, 0, 255)
	DrawRect(8, 8, test.random, 4, 255, 255, 255, 255)
end sub

sub ObjectStartup
	Stage.TimeEnabled = 1
	SetMusicTrack("Tune.ogg", 0, 1)
	PlayMusic(0)
end sub
"""


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("v3", "v4"):
        sys.exit("usage: make-test-game.py v3|v4 <out.rsdk>|<out.zip>")
    common = {
        "Data/SoundFX/Test/Beep.wav": beep(),
        "Data/Music/Tune.ogg": open(os.path.join(HERE, "tune.ogg"), "rb").read(),
    }
    if sys.argv[1] == "v4":
        files = dict(common, **{
            "Data/Game/GameConfig.bin": v4_game_config(),
            "Data/Stages/Test/StageConfig.bin": v4_stage_config(),
            "Data/Stages/Test/Act1.bin": v4_act(),
            "Data/Scripts/Test/Box.txt": V4_BOX.replace("\n", "\r\n").encode(),
        })
        data = pack("4", files)
    else:
        files = dict(common, **{
            "Data/Game/GameConfig.bin": v3_game_config(),
            "Data/Palettes/MasterPalette.act": v3_palette(),
            "Data/Stages/Test/StageConfig.bin": v3_stage_config(),
            "Data/Stages/Test/Act1.bin": v3_act(),
            "Data/Scripts/Test/Box.txt": V3_BOX.replace("\n", "\r\n").encode(),
        })
        data = pack("3", files)
    if sys.argv[2].endswith(".zip"):
        # the Data folder, in the order a folder lists its files
        with zipfile.ZipFile(sys.argv[2], "w", zipfile.ZIP_DEFLATED) as z:
            for name in sorted(files):
                info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, files[name])
        return
    open(sys.argv[2], "wb").write(data)


if __name__ == "__main__":
    main()
