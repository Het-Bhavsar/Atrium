import numpy as np
import pytest

from atrium_ml import hrv, logrec


def _epoch(**kw):
    e = logrec.Epoch(pulse_bpm=58.4, pulse_conf=0.8, on_skin=True, rmssd_ms=41.6, n_diffs=27,
                     activity_class="still", activity_mg=6.2, mlc_level_max=0, pulse_present_s=30,
                     skin_c=33.47, steps=0, pitch_deg=-12.4, roll_deg=87.0, burst_r=0.47, burst_conf=0.9)
    for k, v in kw.items():
        setattr(e, k, v)
    return e


def test_record_is_12_bytes_and_round_trips():
    raw = logrec.encode(_epoch())
    assert len(raw) == logrec.RECORD_SIZE == 12
    d = logrec.decode(raw)
    assert d.pulse_bpm == 58 and d.on_skin and not d.pulse_from_ir
    assert d.rmssd_ms == 42 and d.n_diffs == 27 and d.activity_class == "still"
    assert d.skin_c == pytest.approx(33.5) and d.pitch_deg == -12 and d.roll_deg == 88
    assert d.burst_r == pytest.approx(0.47) and d.burst_conf == pytest.approx(14 / 15)
    assert d.pulse_present_s == 30 and d.pulse_conf == pytest.approx(12 / 15)


def test_saturation_and_missing_values():
    d = logrec.decode(logrec.encode(_epoch(rmssd_ms=400, n_diffs=80, activity_mg=900, steps=400, pulse_present_s=45,
                                           skin_c=float("nan"), pitch_deg=float("nan"), burst_r=0.0)))
    assert d.rmssd_ms == 254 and d.n_diffs == 63 and d.activity_mg == 255 and d.steps == 255
    assert d.pulse_present_s == 30
    assert np.isnan(d.skin_c) and np.isnan(d.pitch_deg) and d.burst_r == 0 and d.burst_conf == 0
    assert logrec.decode(logrec.encode(_epoch(rmssd_ms=0.3))).rmssd_ms == 1   # a real value never reads as none


def test_blocks_carry_time_and_split_at_gaps():
    buf = logrec.encode_block(1_700_000_000, 1, [_epoch()] * 64) + logrec.encode_block(1_700_009_000, 2, [_epoch()] * 3)
    t, up, ep = logrec.decode_blocks(buf)
    assert len(ep) == 67 and not up.any()
    assert t[63] == 1_700_000_000 + 63 * 30 and t[64] == 1_700_009_000 and t[-1] == 1_700_009_060
    with pytest.raises(ValueError):
        logrec.decode_blocks(buf[:-5])


def test_pages_hold_340_records_and_blocks_never_cross_a_page():
    assert logrec.records_per_page() == 340
    buf = logrec.pack_pages([(1_700_000_000, [_epoch()] * 700)])
    assert len(buf) == 3 * logrec.PAGE
    t, _, ep = logrec.decode_blocks(buf)
    assert len(ep) == 700 and np.all(np.diff(t) == 30)
    for p in range(3):          # every page starts with a block header carrying that epoch's time
        t0 = int.from_bytes(buf[p * logrec.PAGE:p * logrec.PAGE + 4], "little")
        assert t0 == 1_700_000_000 + 30 * 340 * p


def test_gap_starts_a_new_block_and_erased_tail_is_skipped():
    buf = logrec.pack_pages([(1_000, [_epoch()] * 10), (5_000, [_epoch()] * 5)])
    assert buf[-1] == 0xFF
    t, _, ep = logrec.decode_blocks(buf)
    assert len(ep) == 15 and t[9] == 1_270 and t[10] == 5_000


def test_open_block_is_read_by_counting_records():
    page = bytearray(logrec.pack_pages([(2_000, [_epoch()] * 7)]))
    page[4:8] = b"\xff\xff\xff\xff"          # second header word not yet written
    t, _, ep = logrec.decode_blocks(bytes(page))
    assert len(ep) == 7 and t[-1] == 2_000 + 6 * 30


def test_uptime_time_before_the_clock_is_set():
    buf = logrec.pack_pages([(logrec.UPTIME_BIT | 120, [_epoch()] * 2)])
    t, up, _ = logrec.decode_blocks(buf)
    assert up.all() and t[0] == 120 and t[1] == 150


def test_reserved_bits_are_zero_in_written_records():
    assert all(logrec.encode(_epoch(burst_conf=c))[11] >> 4 == 0 for c in (0.0, 0.5, 1.0))


def test_capacity_by_log_pages():
    lo, hi = logrec.capacity_hours(16 * 1024)
    assert lo == pytest.approx(8.5) and hi == pytest.approx(11.33, abs=0.01)
    lo, hi = logrec.capacity_hours(32 * 1024)
    assert lo > 19.5 and hi < 23


def test_nightly_rmssd_survives_1_ms_steps():
    rng = np.random.default_rng(1)
    true = [(r, n) for r, n in zip(rng.uniform(20, 80, 900), rng.integers(20, 35, 900))]
    stored = [(logrec.decode(logrec.encode(_epoch(rmssd_ms=r, n_diffs=n))).rmssd_ms, n) for r, n in true]
    assert abs(hrv.pooled(stored) - hrv.pooled(true)) < 0.1


def test_night_inputs_only_pool_still_epochs():
    eps = [_epoch(), _epoch(activity_class="other", rmssd_ms=90)]
    inp = logrec.night_inputs([logrec.decode(logrec.encode(e)) for e in eps])
    assert len(inp["hrv_pairs"]) == 1 and len(inp["bursts"]) == 2 and inp["act_mg"].shape == (2,)
