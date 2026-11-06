"""Compact pod log for the EVT baseline: one 12-byte record per 30 s epoch, day and night.

The log shares the pod's storage partition with settings and BLE bonds, and flash erases in 4 KB pages, so the
log is a ring of whole pages: when it is full, the oldest page is erased. With 4 log pages (16 KB) it holds 8.5-11.3 h
(3 to 4 full pages); with 8 pages (32 KB), 19.8-22.6 h. `capacity_hours` gives the range. The epoch is the one
sleep.py already uses, so the phone-side models run on it unchanged.

Blocks: an 8-byte header, then up to 255 records back to back:
  u32 time of the first epoch | u16 block sequence | u8 format (1) | u8 record count
 - A block never crosses a 4 KB page. When fewer than 20 bytes (header + one record) are left in a page, the rest of
   the page stays erased (0xFF) and the next block opens at the start of the next page.
 - Firmware opens a new block at any gap (dock, sampling stopped, reboot), when a block reaches 255 records, and at a
   page boundary. Epoch k of a block starts at time + 30 k.
 - Flash cannot be rewritten without an erase, so firmware writes the time at open and leaves the second header
   word (sequence, format, count) erased until the block closes. An open block (second word 0xFFFFFFFF) is read by
   counting records: byte 11 bits 4-7 are always 0 in a written record and 0xF in erased flash.
 - Time: Unix seconds once the phone has set the clock. Before that, seconds since boot with bit 31 set; the app
   rebases those blocks on the first connect from the pod's uptime, and drops any from an earlier boot.

Record (little-endian, 12 bytes):
  0  u8  pulse, bpm, mean of the epoch's confident seconds; 0 = none
  1  u8  bits 0-3 pulse confidence x 15 | bit 4 on skin | bit 5 pulse from IR | bit 6 docked | bit 7 charging
  2  u8  RMSSD over the epoch, ms (1 ms steps, 254 = 254 or more); 0 = fewer than 5 clean differences
  3  u8  bits 0-5 clean successive RR differences used (0-63) | bits 6-7 activity class (0 still, 1 walk,
         2 run, 3 other)
  4  u8  activity index, mg (mean of | |a| - 1 g | over the epoch), saturating at 255
  5  u8  bits 0-2 highest MLC motion level | bits 3-7 pulse-present seconds (0-30)
  6  u8  skin temperature TH1: (T - 25.0 C) / 0.1, 1-254 = 25.1-50.4 C; 0 = invalid or below range
  7  u8  steps in the epoch, saturating at 255
  8  i8  head pitch, 1 degree steps; -128 = unknown
  9  i8  head roll, 2 degree steps (-90..90 = -180..180 degrees); -128 = unknown
  10 u8  blood-oxygen burst R x 100, for a burst that ended in this epoch; 0 = none
  11 u8  bits 0-3 burst confidence x 15 (0 = none) | bits 4-7 reserved, always 0 (marks a written record)
"""
import struct
from dataclasses import dataclass

import numpy as np

EPOCH_S = 30
RECORD_SIZE = 12
HEADER_SIZE = 8
BLOCK_RECORDS = 255
PAGE = 4096
FORMAT = 1
UPTIME_BIT = 1 << 31
CLASSES = ("still", "walk", "run", "other")


@dataclass
class Epoch:
    pulse_bpm: float = 0.0          # 0 = none
    pulse_conf: float = 0.0         # 0..1
    on_skin: bool = False
    pulse_from_ir: bool = False
    docked: bool = False
    charging: bool = False
    rmssd_ms: float = 0.0           # 0 = none
    n_diffs: int = 0
    activity_class: str = "still"
    activity_mg: float = 0.0
    mlc_level_max: int = 0
    pulse_present_s: int = 0
    skin_c: float = float("nan")
    steps: int = 0
    pitch_deg: float = float("nan")
    roll_deg: float = float("nan")
    burst_r: float = 0.0            # 0 = no burst ended in this epoch
    burst_conf: float = 0.0


def _u8(v, lo=0, hi=255):
    return int(np.clip(round(v), lo, hi))


def encode(e):
    b0 = _u8(e.pulse_bpm)
    b1 = (_u8(e.pulse_conf * 15, 0, 15) | (e.on_skin << 4) | (e.pulse_from_ir << 5) | (e.docked << 6)
          | (e.charging << 7))
    b2 = _u8(e.rmssd_ms, 0, 254) if e.rmssd_ms > 0 else 0
    if e.rmssd_ms > 0 and b2 == 0:
        b2 = 1
    b3 = _u8(e.n_diffs, 0, 63) | (CLASSES.index(e.activity_class) << 6)
    b4 = _u8(e.activity_mg)
    b5 = _u8(e.mlc_level_max, 0, 7) | (_u8(e.pulse_present_s, 0, 30) << 3)
    b6 = 0 if not np.isfinite(e.skin_c) or e.skin_c < 25.05 else _u8((e.skin_c - 25.0) / 0.1, 1, 254)
    b7 = _u8(e.steps)
    b8 = -128 if not np.isfinite(e.pitch_deg) else int(np.clip(round(e.pitch_deg), -90, 90))
    b9 = -128 if not np.isfinite(e.roll_deg) else int(np.clip(round(e.roll_deg / 2), -90, 90))
    b10 = _u8(e.burst_r * 100, 1, 255) if e.burst_r > 0 else 0
    b11 = _u8(e.burst_conf * 15, 0, 15) if b10 else 0
    return struct.pack("<BBBBBBBBbbBB", b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11)


def decode(raw):
    b = struct.unpack("<BBBBBBBBbbBB", raw)
    return Epoch(
        pulse_bpm=float(b[0]), pulse_conf=(b[1] & 0x0F) / 15.0, on_skin=bool(b[1] & 0x10),
        pulse_from_ir=bool(b[1] & 0x20), docked=bool(b[1] & 0x40), charging=bool(b[1] & 0x80),
        rmssd_ms=float(b[2]), n_diffs=b[3] & 0x3F, activity_class=CLASSES[b[3] >> 6], activity_mg=float(b[4]),
        mlc_level_max=b[5] & 0x07, pulse_present_s=b[5] >> 3,
        skin_c=25.0 + 0.1 * b[6] if b[6] else float("nan"), steps=b[7],
        pitch_deg=float("nan") if b[8] == -128 else float(b[8]),
        roll_deg=float("nan") if b[9] == -128 else 2.0 * b[9],
        burst_r=b[10] / 100.0, burst_conf=(b[11] & 0x0F) / 15.0 if b[10] else 0.0)


def encode_block(t0, seq, epochs):
    if not 0 < len(epochs) <= BLOCK_RECORDS:
        raise ValueError("1-255 records per block")
    return struct.pack("<IHBB", int(t0), seq & 0xFFFF, FORMAT, len(epochs)) + b"".join(encode(e) for e in epochs)


def pack_pages(runs, seq0=0):
    """Firmware's write rule, for vectors and tests. runs: list of (t0, [Epoch, ...]), one per gap-free run.
    Returns whole 4 KB pages, unused bytes left erased (0xFF)."""
    pages, page, seq = [], bytearray(), seq0

    def flush():
        nonlocal page
        if page:
            pages.append(bytes(page) + b"\xff" * (PAGE - len(page)))
            page = bytearray()

    for t0, epochs in runs:
        k = 0
        while k < len(epochs):
            if PAGE - len(page) < HEADER_SIZE + RECORD_SIZE:
                flush()
            room = (PAGE - len(page) - HEADER_SIZE) // RECORD_SIZE
            n = min(room, BLOCK_RECORDS, len(epochs) - k)
            page += encode_block(t0 + EPOCH_S * k, seq, epochs[k:k + n])
            seq += 1
            k += n
    flush()
    return b"".join(pages)


def decode_blocks(buf):
    """Split downloaded pages into (epoch start times, uptime flags, Epoch list). Handles erased page tails and a
    block still open (second header word erased). A buffer that is not whole pages is read as one page."""
    times, uptime, out = [], [], []
    pages = [buf[i:i + PAGE] for i in range(0, len(buf), PAGE)] if len(buf) % PAGE == 0 else [buf]
    for pg in pages:
        i = 0
        while i + HEADER_SIZE + RECORD_SIZE <= len(pg):
            t0, w1 = struct.unpack_from("<II", pg, i)
            if t0 == 0xFFFFFFFF:
                break                                   # erased: the rest of the page is empty
            if w1 == 0xFFFFFFFF:                        # open block: count written records
                n = 0
                while i + HEADER_SIZE + (n + 1) * RECORD_SIZE <= len(pg) and \
                        pg[i + HEADER_SIZE + n * RECORD_SIZE + 11] >> 4 == 0:
                    n += 1
            else:
                _, _, fmt, n = struct.unpack_from("<IHBB", pg, i)
                if fmt != FORMAT or n == 0 or i + HEADER_SIZE + n * RECORD_SIZE > len(pg):
                    raise ValueError(f"bad block at byte {i}")
            i += HEADER_SIZE
            for k in range(n):
                raw = pg[i:i + RECORD_SIZE]
                if raw[11] >> 4:
                    raise ValueError(f"reserved bits set at byte {i}")
                out.append(decode(raw))
                times.append((t0 & ~UPTIME_BIT) + EPOCH_S * k)
                uptime.append(bool(t0 & UPTIME_BIT))
                i += RECORD_SIZE
            if w1 == 0xFFFFFFFF:
                break
    return np.array(times), np.array(uptime, bool), out


def records_per_page():
    """Records in one page written without gaps: a 255-record block, then as many as fit."""
    first = BLOCK_RECORDS
    rest = (PAGE - HEADER_SIZE - first * RECORD_SIZE - HEADER_SIZE) // RECORD_SIZE
    return first + max(rest, 0)


def capacity_hours(log_bytes):
    """(hours just after erasing the oldest page, hours just before), for a gap-free log of whole pages."""
    pages = log_bytes // PAGE
    per = records_per_page() * EPOCH_S / 3600.0
    return (pages - 1) * per, pages * per


def night_inputs(epochs):
    """What the phone models take from a night: sleep features (activity, pulse), HRV pooling pairs,
    gravity (pitch, roll), skin temperature and blood-oxygen bursts."""
    act = np.array([e.activity_mg for e in epochs])
    pulse = np.array([e.pulse_bpm if e.pulse_bpm > 0 else np.nan for e in epochs])
    hrv_pairs = [(e.rmssd_ms, e.n_diffs) for e in epochs
                 if e.rmssd_ms > 0 and e.n_diffs >= 5 and e.activity_class == "still"]
    pr = np.array([(e.pitch_deg, e.roll_deg) for e in epochs])
    skin = np.array([e.skin_c for e in epochs])
    bursts = [(e.burst_r, e.burst_conf) for e in epochs if e.burst_r > 0]
    return {"act_mg": act, "hr_bpm": pulse, "hrv_pairs": hrv_pairs, "pitch_roll": pr, "skin_c": skin,
            "bursts": bursts}
