#!/usr/bin/env python3
"""Writes a .chimeraProject for run-frontend.sh, by hand, as the wizard would.

A project of this core is its settings, its firmware pin (the game's data
file, by SHA-1) and an input log in the panel's own button names. The movie, if
given, is a frame a line, player 1's buttons held as letters (U D L R, A B
C, X Y Z, S Start, s Select; # comments), turned into Chimera's log rows.

usage: make-project.py <package> <out.chimeraProject> <frames> --data <pack> --firmware-id <id>
                       [--movie <movie>] [--settings <json>] [--log-out <path>]
--log-out also writes the rows alone, as chimera-run takes a movie.
"""
import argparse
import hashlib
import json
import zipfile


def inputs_of(log):
    """A movie's text block ("[Input]", "LogKey:...", its entries, "[/Input]")
    as a project file's "inputs": the key's groups of column names, and one
    entry a frame. A group starts where a '#' begins a name; a name ends at
    '|' (as Chimera splits a key)."""
    lines = log.splitlines()
    out = {}
    key = next((l[len("LogKey:"):] for l in lines if l.startswith("LogKey:")), None)
    if key is not None:
        groups, at = [], 0
        while at < len(key):
            assert key[at] == "#", key
            groups.append([])
            at += 1
            while at < len(key) and key[at] != "#":
                bar = key.index("|", at)
                groups[-1].append(key[at:bar])
                at = bar + 1
        out["columns"] = groups
    out["entries"] = [l for l in lines if l.startswith("|")]
    return out

LETTERS = {"Up": "U", "Down": "D", "Left": "L", "Right": "R", "A": "A", "B": "B", "C": "C",
           "X": "X", "Y": "Y", "Z": "Z", "Start": "S", "Select": "s", "Dev Menu": "E"}


def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("package")
    ap.add_argument("out")
    ap.add_argument("frames", type=int)
    ap.add_argument("--data", required=True)
    ap.add_argument("--movie")
    ap.add_argument("--settings", default="{}")
    ap.add_argument("--log-out")
    ap.add_argument("--firmware-id", required=True, help="the firmware entry the data pack is (the one the game setting needs)")
    a = ap.parse_args()

    cfg = json.loads(zipfile.ZipFile(a.package).read("waterbox.config"))
    settings = {d["name"]: d["default"] for d in cfg["settings"]}
    settings.update(json.loads(a.settings))
    buttons = cfg["input"]["buttons"]
    # the players' pads, then the console's control (Dev Menu): the order the
    # package declares them, which a log with no LogKey is read in
    groups = [[b for b in buttons if b.startswith("P1 ")], [b for b in buttons if b.startswith("P2 ")],
              [b for b in buttons if not b.startswith(("P1 ", "P2 "))]]
    rows = []
    if a.movie:
        for line in open(a.movie):
            if line.startswith("#"):
                continue
            held = line.strip()
            # player 1's buttons and the console's are the movie's letters
            rows.append(["".join(LETTERS[b[3:]] if b in groups[0] and LETTERS[b[3:]] in held else
                                 LETTERS[b] if b in groups[2] and LETTERS[b] in held else "." for b in g)
                         for g in groups])
    while len(rows) < a.frames:
        rows.append(["." * len(g) for g in groups])
    rows = ["|" + "|".join(r) + "|" for r in rows[:a.frames]]
    key = "LogKey:" + "".join("#" + "".join(b + "|" for b in g) for g in groups)
    if a.log_out:
        # with its LogKey, so chimera-run reads the columns by name (the
        # Dev Menu column among them)
        open(a.log_out, "w").write(key + "\n" + "\n".join(rows) + "\n")
    log = "[Input]\n" + key + "\n" + "\n".join(rows) + "\n[/Input]\n"

    project = {
        "id": "rsdkv5u-frontend-gate",
        "title": "Sonic Origins core through Chimera",
        "description": "written by waterbox/tests/run-frontend.sh",
        "core": {"name": cfg["coreName"], "version": cfg["version"], "sha1": sha1(a.package)},
        "rerecords": 0,
        "files": [],
        "settings": settings,
        "firmware": [{"id": d["id"], "sha1": sha1(a.data)} for d in cfg["firmware"] if d["id"] == a.firmware_id],
        "coreCache": [],
        "inputs": inputs_of(log),
        "markers": [],
        "branches": [],
        "headers": {
            "MovieVersion": "Chimera Project File v1.1",
            "Platform": cfg["systemId"],
            "SHA1": "",
            "LastInputFrame": str(a.frames - 1),
            "VsyncNumerator": str(cfg["video"]["vsyncNumerator"]),
            "VsyncDenominator": str(cfg["video"]["vsyncDenominator"]),
        },
    }
    with open(a.out, "w") as f:
        json.dump(project, f, indent="\t")


if __name__ == "__main__":
    main()
