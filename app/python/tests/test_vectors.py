import csv

import numpy as np

import make_test_vectors


def test_vectors_match_their_truth(tmp_path):
    names = make_test_vectors.write(tmp_path)
    truth = {"rest_64bpm": 64.0, "walk_72bpm_steps108": 72.0, "run_140bpm_steps168": 140.0,
             "hrv_rest_58bpm_rmssd45": 58.0}
    for name in names:
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
