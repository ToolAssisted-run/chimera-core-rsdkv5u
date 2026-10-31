#!/usr/bin/env python3
"""For tests only: rewrite a data pack of the standalone releases (the 2013
remasters of Sonic 1 and 2, made for the Retro Engine v4, and the 2011 Sonic
CD, made for the v3) in the layout the v5U engine reads, so that the tests
can play real game data. The core does not do this itself and refuses
these packs as they come (docs/PLAN.md, "The standalone packs").

usage: convert-pack.py <data.rsdk of the release> <out file>

- A v4 pack ("RSDKvB") is already in the v5 layout, but its files are
  encrypted with v4's keys (from the file's size) and v5U decrypts with v5's
  (from the file's name): every encrypted file is decrypted and stored
  plain, and the header says "RSDKv4".
- A v3 pack has no header and a layout of its own (folders, then each
  folder's files, names and data encrypted v3's way): every file is
  decrypted and written into a v5-layout pack headed "RSDKv3", found by the
  MD5 of its lower-case path as v5U looks files up.

Of Sonic CD's three packs only the mobile one (data_mobile.rsdk) holds the
scripts in the form v5U's v3 mode runs (Data/Scripts/ByteCode/GlobalCode.bin
and one file a stage); the PC packs hold the PC form (GS000.bin and the
rest), which v5U does not read: they convert, but the engine shows nothing.
The output stays outside the repository, like the pack.
"""
import hashlib
import struct
import sys


def v4_keys(n):
    d = hashlib.md5(str(n).encode()).digest()
    return bytes(d[i * 4 + (3 - j)] for i in range(4) for j in range(4))


def v4_decrypt(buf, size):
    # RSDKv4-Decompilation Reader.cpp, FileRead
    a = v4_keys(size)
    b = v4_keys((size >> 1) + 1)
    no = (size & 0x1FC) >> 2
    pa, pb, swap = 0, 8, 0
    out = bytearray(len(buf))
    for k, c in enumerate(buf):
        v = b[pb] ^ no ^ c
        if swap:
            v = ((v << 4) + (v >> 4)) & 0xFF
        out[k] = v ^ a[pa]
        pa += 1
        pb += 1
        if pa <= 0x0F:
            if pb > 0x0C:
                pb = 0
                swap ^= 1
        elif pb <= 0x08:
            pa = 0
            swap ^= 1
        else:
            no = (no + 2) & 0x7F
            key1 = (0xAAAAAAAB * no) >> 32
            key2 = (0x24924925 * no) >> 32
            t1 = key2 + (no - key2) // 2
            t2 = key1 // 8 * 3
            if swap:
                swap = 0
                pa = no - t1 // 4 * 7
                pb = no - t2 * 4 + 2
            else:
                swap = 1
                pb = no - t1 // 4 * 7
                pa = no - t2 * 4 + 3
    return bytes(out)


V3_A = b"4RaS9D7KaEbxcp2o5r6t\0"
V3_B = b"3tRaUxLmEaSn\0"


def v3_decrypt(buf, size):
    # RSDKv3-Decompilation Reader.cpp, ParseVirtualFileSystem and FileRead
    no = (size & 0x1FC) >> 2
    pb = (no % 9) + 1
    pa = (no % pb) + 1
    swap = 0
    out = bytearray(len(buf))
    for k, c in enumerate(buf):
        v = c ^ V3_B[pb] ^ no
        pb += 1
        if swap:
            v = ((v & 0xF) << 4) | (v >> 4)
        out[k] = (v ^ V3_A[pa]) & 0xFF
        pa += 1
        if pa <= 19 or pb <= 11:
            if pa > 19:
                pa = 1
                swap ^= 1
            if pb > 11:
                pb = 1
                swap ^= 1
        else:
            no = (no + 1) & 0x7F
            if swap:
                swap = 0
                pa = (no % 12) + 6
                pb = (no % 5) + 4
            else:
                swap = 1
                pa = (no % 15) + 3
                pb = (no % 7) + 1
    return bytes(out)


def convert_v4(data):
    data = bytearray(data)
    count = struct.unpack_from("<H", data, 6)[0]
    for f in range(count):
        e = 8 + f * 24
        off, size = struct.unpack_from("<II", data, e + 16)
        if size & 0x80000000:
            size &= 0x7FFFFFFF
            data[off:off + size] = v4_decrypt(data[off:off + size], size)
            struct.pack_into("<I", data, e + 20, size)
    data[5] = ord("4")
    return bytes(data), count


def convert_v3(d):
    header, ndirs = struct.unpack_from("<IH", d, 0)
    p = 6
    dirs = []
    for _ in range(ndirs):
        n = d[p]
        p += 1
        name = bytes(c ^ (0xFF - n) for c in d[p:p + n]).decode("latin-1")
        p += n
        dirs.append((name, struct.unpack_from("<I", d, p)[0]))
        p += 4
    files = []
    for k, (dname, off) in enumerate(dirs):
        end = header + (dirs[k + 1][1] if k + 1 < len(dirs) else len(d) - header)
        q = header + off
        while q < end:
            n = d[q]
            q += 1
            fname = bytes(~c & 0xFF for c in d[q:q + n]).decode("latin-1")
            q += n
            size = struct.unpack_from("<I", d, q)[0]
            q += 4
            files.append((dname + fname, v3_decrypt(d[q:q + size], size)))
            q += size
    out = bytearray(b"RSDKv3" + struct.pack("<H", len(files)))
    base = len(out) + 24 * len(files)
    blobs = bytearray()
    for path, body in files:
        m = hashlib.md5(path.lower().encode("latin-1")).digest()
        out += b"".join(m[i * 4:i * 4 + 4][::-1] for i in range(4))
        out += struct.pack("<II", base + len(blobs), len(body))
        blobs += body
    return bytes(out + blobs), len(files)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    data = open(sys.argv[1], "rb").read()
    if data[:6] == b"RSDKvB":
        out, n = convert_v4(data)
        kind = "v4 (2013)"
    elif data[:4] != b"RSDK":
        out, n = convert_v3(data)
        kind = "v3 (2011)"
    else:
        sys.exit(f"{sys.argv[1]}: already in the v5 layout ({data[:6].decode('latin-1')})")
    open(sys.argv[2], "wb").write(out)
    print(f"{kind} pack, {n} files, written as {out[:6].decode()}")


if __name__ == "__main__":
    main()
