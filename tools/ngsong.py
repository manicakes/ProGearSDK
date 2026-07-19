#!/usr/bin/env python3
"""
ngsong.py - Compile sequenced songs into Z80-resident data for the YM2610.

Streamed ADPCM-B music costs about 9.25 KB per second, so the whole 16 MB
V-ROM holds only ~30 minutes of audio - shared with every sound effect. A
sequenced song is note data instead: a few KB in M1 ROM regardless of length,
with samples used only for drums. This compiler turns an MML song description
into the binary the Z80 sequencer in hal/z80/driver.s plays.

Output layout (patched into M1 ROM at SONG_BASE):

    +0x000  song table: 16 entries x 2 bytes, absolute Z80 pointers (0 = unused)
    +0x020  instrument table: 32 bytes per FM patch
    ...     per-song headers and per-track event streams

Song header:
    byte  timer_b      YM2610 Timer B reload for one tick
    byte  n_tracks
    per track (7 bytes):
        byte  type     0 = FM, 1 = ADPCM-A drum
        byte  channel  FM 0-3, or ADPCM-A channel 0-5
        byte  inst     initial FM patch index
        word  stream   absolute pointer to the event stream
        word  loop     absolute pointer to resume at after the end marker

Event stream bytes:
    0x00-0x5F  note index, followed by a duration byte (ticks)
    0x60       rest, followed by a duration byte
    0x61 nn    set FM patch nn
    0x62 vv    set volume (carrier attenuation, 0 loudest .. 127 silent)
    0x63 pp    set pan (0x40 right, 0x80 left, 0xC0 centre)
    0x64 dd    tie: hold the sounding note dd more ticks without retriggering
    0xFF       end of stream, jump to the loop pointer

Usage:
    python3 ngsong.py song.yaml -o song.bin --report
"""

import argparse
import math
import os
import re
import sys

try:
    import yaml
except ImportError:
    print("ngsong.py requires PyYAML", file=sys.stderr)
    raise

# --- Memory layout -------------------------------------------------------

# Driver code occupies 0x0100-0x07FF, the patched sample tables 0x0800-0x093F,
# boot-jingle data up to ~0x0B1C, and the sequencer code sits at 0x0C00. Song
# data starts well clear of all of it.
SONG_BASE = 0x2000       # where the blob is patched into M1 ROM
SONG_TABLE_ENTRIES = 16
INST_RECORD_SIZE = 32    # 30 patch bytes + carrier mask, padded for <<5 indexing
MAX_INSTRUMENTS = 32

# --- YM2610 constants ----------------------------------------------------

FM_CLOCK = 8_000_000
FM_SAMPLE_RATE = FM_CLOCK / 144          # 55555.6 Hz
TIMER_B_TICK_US = 288.0                  # 1152 * (256-N) / 4MHz

# Operator order used by fm_patch_regs in driver.s: OP1, OP3, OP2, OP4
PATCH_OP_ORDER = (0, 2, 1, 3)

# Carriers per algorithm, as operator indices 0..3 meaning S1..S4
ALG_CARRIERS = {
    0: (3,), 1: (3,), 2: (3,), 3: (3,),
    4: (1, 3),
    5: (1, 2, 3), 6: (1, 2, 3),
    7: (0, 1, 2, 3),
}

# TL register slot order matches PATCH_OP_ORDER: 0x40=S1, 0x44=S3, 0x48=S2, 0x4C=S4
TL_SLOT_OF_OP = {0: 0, 2: 1, 1: 2, 3: 3}

# Stream opcodes
OP_REST = 0x60
OP_INST = 0x61
OP_VOL = 0x62
OP_PAN = 0x63
OP_TIE = 0x64
OP_END = 0xFF

# Note indices run C1..C8. Block 7 tops out around 4.2 kHz, so C8 (index 84)
# is the highest representable note; the opcode space allows up to 0x5F.
NOTE_MAX = 84

PAN_VALUES = {"right": 0x40, "left": 0x80, "center": 0xC0, "centre": 0xC0}

SEMITONES = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}


# --- Note table ----------------------------------------------------------

def note_frequency(midi):
    return 440.0 * (2.0 ** ((midi - 69) / 12.0))


def fnum_block(midi):
    """Pick the block that keeps the F-number in its most precise octave."""
    freq = note_frequency(midi)
    for block in range(8):
        # block 0 halves the divisor rather than doubling it, so this has to be
        # a real power, not a shift
        fnum = round(freq * (1 << 20) / FM_SAMPLE_RATE / (2.0 ** (block - 1)))
        # One octave spans 617..1234; the slack absorbs rounding at the top of
        # the range (C8 lands on 1235) and stays well inside the 11-bit field.
        if 617 <= fnum <= 1240:
            return block, fnum
    raise ValueError(f"note {midi} out of range for the YM2610")


# Note index 0 == MIDI 24 (C1); 0x5F notes spans C1..B8
NOTE_BASE_MIDI = 24


def build_note_table():
    table = bytearray()
    for i in range(NOTE_MAX + 1):
        block, fnum = fnum_block(NOTE_BASE_MIDI + i)
        table.append(((block & 7) << 3) | ((fnum >> 8) & 7))
        table.append(fnum & 0xFF)
    return bytes(table)


# --- Instruments ---------------------------------------------------------

OP_FIELD_ORDER = [
    ("dt", "mul"),      # 0x30 : DT<<4 | MUL
    ("tl",),            # 0x40 : TL
    ("ks", "ar"),       # 0x50 : KS<<6 | AR
    ("am", "dr"),       # 0x60 : AM<<7 | DR
    ("sr",),            # 0x70 : SR
    ("sl", "rr"),       # 0x80 : SL<<4 | RR
    ("ssgeg",),         # 0x90 : SSG-EG
]


def pack_operator_byte(op, fields):
    if fields == ("dt", "mul"):
        return ((op.get("dt", 0) & 7) << 4) | (op.get("mul", 0) & 0x0F)
    if fields == ("tl",):
        return op.get("tl", 0) & 0x7F
    if fields == ("ks", "ar"):
        return ((op.get("ks", 0) & 3) << 6) | (op.get("ar", 0) & 0x1F)
    if fields == ("am", "dr"):
        return ((op.get("am", 0) & 1) << 7) | (op.get("dr", 0) & 0x1F)
    if fields == ("sr",):
        return op.get("sr", 0) & 0x1F
    if fields == ("sl", "rr"):
        return ((op.get("sl", 0) & 0x0F) << 4) | (op.get("rr", 0) & 0x0F)
    if fields == ("ssgeg",):
        return op.get("ssgeg", 0) & 0x0F
    raise AssertionError(fields)


def compile_instrument(name, spec):
    ops = spec.get("ops")
    if not ops or len(ops) != 4:
        raise ValueError(f"instrument '{name}' needs exactly 4 operators")
    alg = spec.get("alg", 0) & 7
    fb = spec.get("fb", 0) & 7

    data = bytearray()
    for fields in OP_FIELD_ORDER:
        for op_index in PATCH_OP_ORDER:
            data.append(pack_operator_byte(ops[op_index], fields))
    data.append((fb << 3) | alg)                 # 0xB0 feedback/algorithm
    data.append(PAN_VALUES[spec.get("pan", "center")])  # 0xB4 L/R

    assert len(data) == 30, len(data)

    mask = 0
    for op_index in ALG_CARRIERS[alg]:
        mask |= 1 << TL_SLOT_OF_OP[op_index]

    record = bytearray(INST_RECORD_SIZE)
    record[0:30] = data
    record[30] = mask
    return bytes(record)


# --- MML parser ----------------------------------------------------------

class MMLError(Exception):
    pass


class MMLParser:
    """
    Subset of MML sufficient for writing a full arrangement:

        cdefgab   note, optional + / - accidental, optional length, optional dots
        r         rest
        o<n> < >  octave set / down / up
        l<n>      default note length
        v<n>      volume (carrier attenuation, 0 loudest)
        @<n>      FM patch index
        p<l|c|r>  pan
        &         tie the previous note into the next duration
        [ ... ]n  repeat the enclosed block n times
        |         bar separator, ignored (readability only)
    """

    def __init__(self, text, ticks_per_beat, drum_map=None):
        self.text = text
        self.pos = 0
        self.tpb = ticks_per_beat
        self.drum_map = drum_map
        self.octave = 4
        self.default_len = 4
        self.events = []

    # -- low level ------------------------------------------------------
    def peek(self):
        return self.text[self.pos] if self.pos < len(self.text) else ""

    def take(self):
        ch = self.peek()
        self.pos += 1
        return ch

    def read_int(self, default=None):
        start = self.pos
        while self.peek().isdigit():
            self.pos += 1
        if start == self.pos:
            if default is None:
                raise MMLError(f"expected a number at offset {self.pos}")
            return default
        return int(self.text[start:self.pos])

    def read_duration(self):
        """Length number (4 = quarter) plus dots, returned in ticks."""
        length = self.read_int(self.default_len)
        if length <= 0:
            raise MMLError(f"bad note length {length}")
        ticks = (self.tpb * 4) / length
        dotted = ticks
        while self.peek() == ".":
            self.take()
            dotted /= 2
            ticks += dotted
        if abs(ticks - round(ticks)) > 1e-6:
            raise MMLError(f"length {length} does not divide {self.tpb} ticks/beat evenly")
        return int(round(ticks))

    # -- events ---------------------------------------------------------
    def emit_note(self, note_index, ticks):
        self.events.append(("note", note_index, ticks))

    def emit_rest(self, ticks):
        self.events.append(("rest", ticks))

    # -- parsing --------------------------------------------------------
    def parse(self):
        self._parse_until(None)
        return self.events

    def _parse_until(self, terminator):
        while self.pos < len(self.text):
            ch = self.peek()
            if ch in " \t\n\r|":
                self.take()
                continue
            if ch == "]":
                if terminator != "]":
                    raise MMLError("unmatched ']'")
                return
            self.take()

            if ch == "[":
                block_start = self.pos
                depth = 1
                while self.pos < len(self.text) and depth:
                    c = self.text[self.pos]
                    if c == "[":
                        depth += 1
                    elif c == "]":
                        depth -= 1
                    self.pos += 1
                if depth:
                    raise MMLError("unmatched '['")
                inner = self.text[block_start:self.pos - 1]
                count = self.read_int(2)
                for _ in range(count):
                    sub = MMLParser(inner, self.tpb, self.drum_map)
                    sub.octave, sub.default_len = self.octave, self.default_len
                    self.events.extend(sub.parse())
                    self.octave, self.default_len = sub.octave, sub.default_len
                continue

            # Drum letters are matched before the command letters: a drum kit
            # may legitimately use 'o' (open hat) or 'l', which would otherwise
            # be read as octave/length commands.
            if self.drum_map is not None and ch in self.drum_map:
                self.emit_note(self.drum_map[ch], self.read_duration())
                continue

            if ch == "o":
                self.octave = self.read_int()
                continue
            if ch == ">":
                self.octave += 1
                continue
            if ch == "<":
                self.octave -= 1
                continue
            if ch == "l":
                self.default_len = self.read_int()
                continue
            if ch == "v":
                self.events.append(("vol", self.read_int()))
                continue
            if ch == "@":
                self.events.append(("inst", self.read_int()))
                continue
            if ch == "p":
                key = self.take().lower()
                pan = {"l": 0x80, "c": 0xC0, "r": 0x40}.get(key)
                if pan is None:
                    raise MMLError(f"bad pan '{key}'")
                self.events.append(("pan", pan))
                continue
            if ch == "&":
                self.events.append(("tie", self.read_duration()))
                continue
            if ch == "r":
                self.emit_rest(self.read_duration())
                continue

            if self.drum_map is not None:
                raise MMLError(f"unknown drum '{ch}'")

            if ch in SEMITONES:
                semi = SEMITONES[ch]
                while self.peek() in "+-#":
                    semi += 1 if self.take() in "+#" else -1
                ticks = self.read_duration()
                midi = (self.octave + 1) * 12 + semi
                index = midi - NOTE_BASE_MIDI
                if not 0 <= index <= NOTE_MAX:
                    raise MMLError(f"note {ch} octave {self.octave} out of range")
                self.emit_note(index, ticks)
                continue

            raise MMLError(f"unexpected character '{ch}' at offset {self.pos - 1}")


def encode_events(events):
    """Turn parsed events into the byte stream, merging over-long durations."""
    out = bytearray()
    for ev in events:
        kind = ev[0]
        if kind == "note":
            _, index, ticks = ev
            out.append(index)
            out.append(min(ticks, 255))
            ticks -= min(ticks, 255)
            while ticks > 0:                      # very long note: extend with ties
                step = min(ticks, 255)
                out += bytes((OP_TIE, step))
                ticks -= step
        elif kind == "rest":
            ticks = ev[1]
            while ticks > 0:
                step = min(ticks, 255)
                out += bytes((OP_REST, step))
                ticks -= step
        elif kind == "tie":
            ticks = ev[1]
            while ticks > 0:
                step = min(ticks, 255)
                out += bytes((OP_TIE, step))
                ticks -= step
        elif kind == "vol":
            out += bytes((OP_VOL, min(ev[1], 127)))
        elif kind == "inst":
            out += bytes((OP_INST, ev[1] & 0xFF))
        elif kind == "pan":
            out += bytes((OP_PAN, ev[1] & 0xFF))
        else:
            raise AssertionError(kind)
    out.append(OP_END)
    return bytes(out)


def track_tick_length(events):
    total = 0
    for ev in events:
        if ev[0] == "note":
            total += ev[2]
        elif ev[0] in ("rest", "tie"):
            total += ev[1]
    return total


# --- Song compilation ----------------------------------------------------

def timer_b_for_tempo(bpm, ticks_per_beat):
    tick_us = 60_000_000.0 / (bpm * ticks_per_beat)
    counts = tick_us / TIMER_B_TICK_US
    n = 256 - int(round(counts))
    if not 0 <= n <= 255:
        raise ValueError(f"tempo {bpm} @ {ticks_per_beat} ticks/beat needs Timer B {n}")
    actual = 60_000_000.0 / ((256 - n) * TIMER_B_TICK_US * ticks_per_beat)
    return n, actual


def parse_sfx_header(path):
    """Pull NGSFX_<NAME> -> index out of the generated asset header."""
    index = {}
    if not path or not os.path.exists(path):
        return index
    with open(path) as f:
        for line in f:
            m = re.match(r"\s*#define\s+NGSFX_([A-Z0-9_]+)\s+(\d+)", line)
            if m:
                index[m.group(1).lower()] = int(m.group(2))
    return index


def resolve_drums(drums, sfx_index):
    """Map drum letters to ADPCM-A sample indices, by name or literal index."""
    out = {}
    for letter, value in drums.items():
        if isinstance(value, int):
            out[letter] = value
            continue
        key = str(value).lower()
        if key not in sfx_index:
            raise ValueError(
                f"drum '{letter}' refers to sound effect '{value}', which is not in "
                f"the asset header. Known: {', '.join(sorted(sfx_index)) or '(none)'}")
        out[letter] = sfx_index[key]
    return out


def compile_song(doc, report=False, sfx_index=None):
    sfx_index = sfx_index or {}
    tpb = doc.get("ticks_per_beat", 24)
    bpm = doc.get("tempo", 120)
    timer_b, actual_bpm = timer_b_for_tempo(bpm, tpb)

    inst_names = list(doc.get("instruments", {}).keys())
    if len(inst_names) > MAX_INSTRUMENTS:
        raise ValueError(f"{len(inst_names)} instruments exceeds {MAX_INSTRUMENTS}")
    inst_index = {name: i for i, name in enumerate(inst_names)}
    inst_blob = bytearray()
    for name in inst_names:
        inst_blob += compile_instrument(name, doc["instruments"][name])
    inst_blob += bytes(INST_RECORD_SIZE * (MAX_INSTRUMENTS - len(inst_names)))

    drum_map = resolve_drums(doc.get("drums", {}), sfx_index)

    tracks = []
    for spec in doc.get("tracks", []):
        channel = spec["channel"]
        if channel.startswith("fm"):
            ttype, hw = 0, int(channel[2:])
            if not 0 <= hw <= 3:
                raise ValueError(f"FM channel {hw} out of range 0-3")
            parser = MMLParser(spec.get("mml", ""), tpb)
        elif channel.startswith("drum"):
            ttype, hw = 1, int(channel[4:] or 0)
            parser = MMLParser(spec.get("mml", ""), tpb, drum_map=drum_map)
        else:
            raise ValueError(f"unknown channel '{channel}'")

        parser.octave = spec.get("octave", 4)
        events = parser.parse()

        prelude = []
        if "volume" in spec:
            prelude.append(("vol", spec["volume"]))
        if "pan" in spec:
            prelude.append(("pan", PAN_VALUES[spec["pan"]]))
        events = prelude + events

        inst = inst_index.get(spec.get("instrument"), 0) if ttype == 0 else 0
        tracks.append({
            "type": ttype, "hw": hw, "inst": inst,
            "events": events, "name": spec.get("name", channel),
            "ticks": track_tick_length(events),
            "loop_events": len(prelude),
        })

    lengths = {t["name"]: t["ticks"] for t in tracks}
    distinct = set(lengths.values())
    if len(distinct) > 1:
        raise ValueError(
            "all tracks must be the same musical length so the loop stays in sync; got "
            + ", ".join(f"{n}={v}" for n, v in lengths.items()))

    # Layout: header, then each track's stream
    header_size = 2 + 7 * len(tracks)
    song_start = SONG_BASE + 0x20 + len(inst_blob)
    stream_at = song_start + header_size

    streams, loop_offsets = [], []
    for t in tracks:
        prelude_bytes = encode_events(t["events"][:t["loop_events"]])[:-1]
        body = encode_events(t["events"])
        streams.append(body)
        loop_offsets.append(len(prelude_bytes))

    header = bytearray()
    header.append(timer_b)
    header.append(len(tracks))
    cursor = stream_at
    for t, stream, loop_off in zip(tracks, streams, loop_offsets):
        header.append(t["type"])
        header.append(t["hw"])
        header.append(t["inst"])
        header += cursor.to_bytes(2, "little")
        header += (cursor + loop_off).to_bytes(2, "little")
        cursor += len(stream)

    blob = bytearray()
    blob += bytes(0x20)                                   # song table, filled below
    blob += inst_blob
    blob += header
    for stream in streams:
        blob += stream
    blob[0:2] = song_start.to_bytes(2, "little")          # song 0

    if report:
        print(f"tempo {bpm} BPM requested -> Timer B {timer_b} "
              f"({actual_bpm:.2f} BPM actual, {tpb} ticks/beat)")
        print(f"instruments: {len(inst_names)} ({', '.join(inst_names)})")
        beats = tracks[0]["ticks"] / tpb if tracks else 0
        print(f"length: {tracks[0]['ticks']} ticks = {beats:.0f} beats "
              f"= {beats / 4:.0f} bars, {beats * 60 / actual_bpm:.1f} s per loop")
        for t, stream in zip(tracks, streams):
            print(f"  {t['name']:10s} {t['type'] and 'drum' or 'FM ' + str(t['hw']):5s} "
                  f"{len(stream):5d} bytes")
        print(f"total: {len(blob)} bytes at 0x{SONG_BASE:04X} "
              f"(vs ~{beats * 60 / actual_bpm * 9250:,.0f} bytes as streamed ADPCM)")

    return bytes(blob)


def main():
    ap = argparse.ArgumentParser(description="Compile an MML song for the YM2610")
    ap.add_argument("source", help="song YAML")
    ap.add_argument("-o", "--output", required=True, help="output binary blob")
    ap.add_argument("--note-table", help="also write the note table binary here")
    ap.add_argument("--sfx-header",
                    help="generated progear_assets.h, for resolving drum names")
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    with open(args.source) as f:
        doc = yaml.safe_load(f)

    blob = compile_song(doc, report=args.report,
                        sfx_index=parse_sfx_header(args.sfx_header))
    with open(args.output, "wb") as f:
        f.write(blob)

    if args.note_table:
        with open(args.note_table, "wb") as f:
            f.write(build_note_table())

    return 0


if __name__ == "__main__":
    sys.exit(main())
