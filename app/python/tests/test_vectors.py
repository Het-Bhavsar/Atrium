import csv

import numpy as np

import make_test_vectors


def test_vectors_match_their_truth(tmp_path):
    names = make_test_vectors.write(tmp_path)
    truth = {"rest_64bpm": 64.0, "walk_72bpm_steps108": 72.0, "run_140bpm_steps168": 140.0,
             "hrv_rest_58bpm_rmssd45": 58.0}
    for name in [n for n in names if not n.startswith(("mlc_", "spo2_"))]:
        rows = list(csv.DictReader(open(tmp_path / f"{name}_hr.csv")))
        bpm = np.array([float(r["bpm"]) for r in rows])
        present = np.array([int(r["pulse_present"]) for r in rows])
        if name in truth:
            assert abs(np.nanmedian(bpm[12:]) - truth[name]) < 3.0, name
        if name.startswith(("rest", "hrv")):
            assert present[12:].mean() > 0.9, name
        if name.startswith("offbody"):
            assert not present.any()


def test_vectors_are_deterministic(tmp_path):
    make_test_vectors.write(tmp_path / "a")
    make_test_vectors.write(tmp_path / "b")
    for f in (tmp_path / "a").iterdir():
        assert f.read_bytes() == (tmp_path / "b" / f.name).read_bytes()


def test_mlc_and_spo2_vectors(tmp_path):
    make_test_vectors.write(tmp_path)
    want = {"still": (0, "still"), "head_motion": (2, "other"), "walk": (4, "walk"), "run": (6, "run")}
    for case, (level, cls) in want.items():
        lv = [int(r["level"]) for r in csv.DictReader(open(tmp_path / f"mlc_{case}_levels.csv"))]
        cl = [r["class"] for r in csv.DictReader(open(tmp_path / f"mlc_{case}_class.csv"))]
        assert set(lv) == {level}, case
        assert set(cl) == {cls}, case
    cfg = list(csv.DictReader(open(tmp_path / "mlc_config.csv")))
    assert len(cfg) == 97 and cfg[-2] == {"addr": "0x10", "data": "0x10"}
    burst = {c: next(csv.DictReader(open(tmp_path / f"spo2_{c}_burst.csv")))
             for c in ("r045", "r070", "moved", "low_perfusion")}
    assert abs(float(burst["r045"]["r"]) - 0.45) < 0.01
    assert abs(float(burst["r070"]["r"]) - 0.70) < 0.015
    assert float(burst["moved"]["confidence"]) < float(burst["r045"]["confidence"])
    assert burst["low_perfusion"]["r"] == "" and float(burst["low_perfusion"]["confidence"]) == 0.0
