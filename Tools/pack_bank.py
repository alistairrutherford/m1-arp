#!/usr/bin/env python3
"""Pack zone JSON and WAV files into an M1BK bank.

The binary matches Bank::loadFile. Little-endian:

    char magic[4] = "M1BK"
    u32 version = 1
    u32 multiCount
    repeated:
        u16 index
        u16 zoneCount
        repeated:
            i16 root, lo, hi, tuneCents
            u32 loopStart, loopEnd, sampleCount
            i16 samples[sampleCount]
    u32 drumCount
    repeated:
        u16 index
        u32 sampleCount
        i16 samples[sampleCount]

A directory may hold one JSON file or several. A multisample looks like:

    { "index": 31, "zones": [ { "wav": "strings-c2.wav", "root": 36,
        "lo": 36, "hi": 48, "tuneCents": 0, "loopStart": 0, "loopEnd": 0 } ] }

A drum sample is { "index": 2, "drum": true, "wav": "hat.wav" }, or a list
under the key "drums". WAV files are converted to mono 16-bit at 44100 Hz.
"""

import argparse
import json
import struct
import wave
from pathlib import Path


def read_wav (path: Path):
    with wave.open (str (path), "rb") as w:
        channels = w.getnchannels()
        width = w.getsampwidth()
        rate = w.getframerate()
        frames = w.readframes (w.getnframes())
    if width == 1:
        samples = [((b - 128) / 128.0) for b in frames]
    elif width == 2:
        raw = struct.unpack ("<" + "h" * (len (frames) // 2), frames)
        samples = [s / 32768.0 for s in raw]
    elif width == 3:
        samples = []
        for i in range (0, len (frames), 3):
            v = frames[i] | (frames[i + 1] << 8) | (frames[i + 2] << 16)
            if v & 0x800000:
                v -= 0x1000000
            samples.append (v / 8388608.0)
    elif width == 4:
        raw = struct.unpack ("<" + "i" * (len (frames) // 4), frames)
        samples = [s / 2147483648.0 for s in raw]
    else:
        raise SystemExit (f"{path}: unsupported sample width {width}")
    if channels > 1:
        mono = []
        for i in range (0, len (samples), channels):
            mono.append (sum (samples[i:i + channels]) / channels)
        samples = mono
    if rate != 44100 and samples:
        out_n = max (1, int (round (len (samples) * 44100 / rate)))
        out = []
        span = len (samples) - 1
        for i in range (out_n):
            pos = span * (i / max (1, out_n - 1)) if out_n > 1 else 0.0
            a = int (pos)
            b = min (span, a + 1)
            frac = pos - a
            out.append (samples[a] * (1.0 - frac) + samples[b] * frac)
        samples = out
    pcm = []
    for s in samples:
        v = int (round (max (-1.0, min (1.0, s)) * 32767.0))
        pcm.append (max (-32768, min (32767, v)))
    return pcm


def load_documents (directory: Path):
    docs = []
    for path in sorted (directory.glob ("*.json")):
        data = json.loads (path.read_text())
        if isinstance (data, list):
            docs.extend (data)
        else:
            docs.append (data)
    return docs


def pack (directory: Path, dest: Path):
    multis = []
    drums = []
    for doc in load_documents (directory):
        if "drums" in doc and isinstance (doc["drums"], list):
            drums.extend (doc["drums"])
            continue
        if doc.get ("drum"):
            drums.append (doc)
        else:
            multis.append (doc)

    out = bytearray (b"M1BK")
    out += struct.pack ("<II", 1, len (multis))
    for multi in multis:
        zones = multi.get ("zones", [])
        out += struct.pack ("<HH", int (multi["index"]), len (zones))
        for zone in zones:
            wav = directory / zone["wav"]
            pcm = read_wav (wav)
            out += struct.pack (
                "<hhhhIII",
                int (zone.get ("root", 60)),
                int (zone.get ("lo", 0)),
                int (zone.get ("hi", 127)),
                int (zone.get ("tuneCents", 0)),
                int (zone.get ("loopStart", 0)),
                int (zone.get ("loopEnd", 0)),
                len (pcm),
            )
            out += struct.pack ("<" + "h" * len (pcm), *pcm)
    out += struct.pack ("<I", len (drums))
    for drum in drums:
        wav = directory / drum["wav"]
        pcm = read_wav (wav)
        out += struct.pack ("<HI", int (drum["index"]), len (pcm))
        if pcm:
            out += struct.pack ("<" + "h" * len (pcm), *pcm)
    dest.write_bytes (out)
    print (f"wrote {dest} ({len (multis)} multisamples, {len (drums)} drums, {len (out)} bytes)")


def main():
    parser = argparse.ArgumentParser (description="Pack WAV zones into an M1BK bank.")
    parser.add_argument ("directory", type=Path, help="Directory of JSON and WAV files")
    parser.add_argument ("output", type=Path, help="Destination .m1bk file")
    args = parser.parse_args()
    if not args.directory.is_dir():
        raise SystemExit (f"not a directory: {args.directory}")
    pack (args.directory, args.output)


if __name__ == "__main__":
    main()
