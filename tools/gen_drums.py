#!/usr/bin/env python3
"""
gen_drums.py - Synthesize a small ADPCM-A drum kit for sequenced songs.

The sequencer plays pitched parts on the YM2610's FM channels but still needs
samples for percussion. These are short - the whole kit is a few tens of KB of
V-ROM, against ~9.25 KB per second for streamed music - so a sequenced song
with drums stays far cheaper than streaming the same track.

Rendered at 18.5 kHz mono, the ADPCM-A native rate, to avoid a resampling pass.

Usage:
    python3 gen_drums.py -o demos/showcase/assets
"""

import argparse
import math
import os
import random
import struct
import sys
import wave

RATE = 18500


def write_wav(path, samples):
    frames = bytearray()
    for s in samples:
        v = max(-1.0, min(1.0, s))
        frames += struct.pack("<h", int(v * 32767))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(frames))
    return len(samples)


def envelope(i, n, decay):
    """Exponential decay with a short fade-out so samples end at zero."""
    e = math.exp(-decay * i / n)
    tail = min(1.0, (n - i) / (0.05 * n)) if n > i else 0.0
    return e * tail


def kick(dur=0.20):
    n = int(RATE * dur)
    out = []
    phase = 0.0
    for i in range(n):
        # Pitch sweep 130 Hz -> 45 Hz gives the classic "thump"
        f = 45.0 + (130.0 - 45.0) * math.exp(-7.0 * i / n)
        phase += 2 * math.pi * f / RATE
        body = math.sin(phase)
        click = math.sin(2 * math.pi * 1400 * i / RATE) * math.exp(-90.0 * i / n)
        out.append((body * 0.92 + click * 0.15) * envelope(i, n, 4.2))
    return out


def snare(dur=0.16):
    n = int(RATE * dur)
    rng = random.Random(0x5EED)
    out = []
    hp = 0.0
    for i in range(n):
        white = rng.uniform(-1.0, 1.0)
        hp = 0.72 * hp + 0.28 * white            # gentle low-pass on the noise
        noise = white - hp                        # ...subtracted back out = high-pass
        tone = (math.sin(2 * math.pi * 190 * i / RATE) * 0.6 +
                math.sin(2 * math.pi * 278 * i / RATE) * 0.4)
        out.append((noise * 0.78 + tone * 0.34) * envelope(i, n, 5.5))
    return out


def hat(dur, decay):
    n = int(RATE * dur)
    rng = random.Random(0x1234)
    out = []
    prev = 0.0
    for i in range(n):
        white = rng.uniform(-1.0, 1.0)
        # Difference of successive noise = bright, metallic high-pass
        val = white - prev
        prev = white
        out.append(val * 0.62 * envelope(i, n, decay))
    return out


def main():
    ap = argparse.ArgumentParser(description="Generate ADPCM-A drum samples")
    ap.add_argument("-o", "--outdir", required=True)
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    kit = {
        "kick": kick(),
        "snare": snare(),
        "hihat_closed": hat(0.055, 16.0),
        "hihat_open": hat(0.26, 4.5),
    }

    total = 0
    for name, samples in kit.items():
        path = os.path.join(args.outdir, f"{name}.wav")
        n = write_wav(path, samples)
        # ADPCM-A is 4 bits per sample
        total += n // 2
        if args.report:
            print(f"  {name:14s} {n:6d} samples  {n / RATE * 1000:6.1f} ms  "
                  f"~{n // 2:6d} bytes as ADPCM-A")
    if args.report:
        print(f"kit total: ~{total:,} bytes of V-ROM")
    return 0


if __name__ == "__main__":
    sys.exit(main())
