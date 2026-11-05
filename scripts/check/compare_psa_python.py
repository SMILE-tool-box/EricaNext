#!/usr/bin/env python3
"""Compare PSA_TreeWriter/psa of EricaNext with psa_decode.py + pixel_id.py (RBFInterpolator), hit by hit.

    /usr/bin/python3 compare_psa_python.py <out.root> <run dir> <board> <psa_param.csv> <position_haNN.csv> [--constpeak] [--detail]

<run dir> holds ha<NN>_*.dat of <board> (the same data as the EricaNext run). The reference decodes them
with the functions of ~/analysis_sm3_FM/PSA_analysis/src_onoda/psa_decode.py ($PSA_SRC to change) and maps
x_hg, y_hg with RBFInterpolator(thin_plate_spline) through the 64 points (x, y) -> (n, m) of the position file.
--constpeak: the run was made with PSA_CalcFADC.fixed_window = true.
--detail: for x_hg / x_lg, the number of rows that differ and the largest |hg| / |lg| of them.
Expected: identical trig, ch, hitnum, peak_id, satu_h/l, pix_n/m; hg etc. within float32 rounding
(relative 1e-6; x, y differ only for hits with |hg| or |lg| near 0).
Needs numpy, scipy, h5py, numba, natsort and PyROOT: /usr/bin/python3 on this machine.
"""
import glob
import os
import sys

sys.path.insert(0, os.path.expanduser("~"))
sys.path.insert(0, os.environ.get("PSA_SRC", os.path.expanduser("~/analysis_sm3_FM/PSA_analysis/src_onoda")))
import numpy as np
import ROOT
from scipy.interpolate import RBFInterpolator

import psa_decode as pd
from sub_scr.get_info import get_temp_calibration, load_psa_config

root_path, run_dir, board, param, position = sys.argv[1:6]
constpeak = "--constpeak" in sys.argv
board = int(board)
ha = f"ha{board:02d}"

unit = [u for u in load_psa_config({"psa_param_dir": param}) if u["HA"].lower() == ha][0]
ta, tb = get_temp_calibration(unit)

ref = {k: [] for k in ["trig", "ch", "hitnum", "hg", "lg", "x_hg", "y_hg", "x_lg", "y_lg",
                       "peak_id", "satu_h", "satu_l", "wave_sum", "ped", "temp"]}
for f in sorted(glob.glob(f"{run_dir}/{ha}_*.dat")):
    buf, sizes, n = pd.read_file(f)
    (trig, clk, hit, temp_raw, temp_corr, ped, wave_sum, hg, lg, x_hg, y_hg, x_lg, y_lg,
     peak_id, hitnum, satu_h, satu_l, _) = pd.decode_events(buf, sizes, constpeak, ta, tb, False)
    ev, j = np.where(hit == 1)
    ref["trig"].append(trig[ev]); ref["ch"].append(5 - j); ref["hitnum"].append(hitnum[ev])
    for k, a in [("hg", hg), ("lg", lg), ("x_hg", x_hg), ("y_hg", y_hg), ("x_lg", x_lg), ("y_lg", y_lg),
                 ("peak_id", peak_id), ("satu_h", satu_h), ("satu_l", satu_l), ("wave_sum", wave_sum), ("ped", ped)]:
        ref[k].append(a[ev, j])
    ref["temp"].append(temp_corr[ev])
ref = {k: np.concatenate(v) for k, v in ref.items()}

# pixel_id.py, with the n, m of the position file
pos = np.genfromtxt(position, delimiter=",", names=True)
xc = np.full(len(ref["ch"]), np.nan); yc = xc.copy()
for ch in range(6):
    p = pos[pos["ch"] == ch]
    sel = ref["ch"] == ch
    if len(p) != 64 or not sel.any():
        continue
    w = RBFInterpolator(np.column_stack([p["x"], p["y"]]), np.column_stack([p["n"], p["m"]]),
                        kernel="thin_plate_spline", smoothing=0)
    c = w(np.column_stack([ref["x_hg"][sel], ref["y_hg"][sel]]).astype(float))
    xc[sel], yc[sel] = c[:, 0], c[:, 1]
pn = np.floor(xc + 0.5); pm = np.floor(yc + 0.5)
out = (pn < 1) | (pn > 8) | (pm < 1) | (pm > 8) | np.isnan(xc)
pn[out] = -1; pm[out] = -1
ref.update(x_corr=xc, y_corr=yc, pix_n=pn, pix_m=pm)

# EricaNext
rdf = ROOT.RDataFrame("PSA_TreeWriter/psa", root_path).Filter(f"board == {board}")
cols = ["trig", "ch", "hitnum", "hg", "lg", "x_hg", "y_hg", "x_lg", "y_lg", "peak_id", "satu_h", "satu_l",
        "x_corr", "y_corr", "pix_n", "pix_m"]
d = rdf.AsNumpy(cols + ["wave_sum", "ped", "temp"])
erica = {k: np.asarray(d[k]) for k in cols}
for k in ["wave_sum", "ped", "temp"]:
    erica[k] = np.array([np.asarray(v) for v in d[k]])

print(f"rows: psa_decode {len(ref['trig'])}, EricaNext {len(erica['trig'])}")
oa = np.lexsort((ref["ch"], ref["trig"])); ob = np.lexsort((erica["ch"], erica["trig"]))
ref = {k: v[oa] for k, v in ref.items()}; erica = {k: v[ob] for k, v in erica.items()}
if len(oa) != len(ob):
    sa = set(zip(ref["trig"], ref["ch"])); sb = set(zip(erica["trig"], erica["ch"]))
    print("only psa_decode:", sorted(sa - sb)[:10], "only EricaNext:", sorted(sb - sa)[:10])
    sys.exit(1)
assert (ref["trig"] == erica["trig"]).all() and (ref["ch"] == erica["ch"]).all()
print("trig, ch: identical")
for k in ["hitnum", "peak_id", "satu_h", "satu_l", "pix_n", "pix_m"]:
    bad = (ref[k].astype(int) != erica[k].astype(int))
    print(f"  {k:9s} mismatches {bad.sum()}" + (f"  e.g. rows {np.where(bad)[0][:5]}" if bad.any() else ""))
for k in ["hg", "lg", "wave_sum", "ped", "temp", "x_hg", "y_hg", "x_lg", "y_lg", "x_corr", "y_corr"]:
    a, b = ref[k].astype(float), erica[k].astype(float)
    ok = ~(np.isnan(a) & np.isnan(b))
    diff = np.abs(a - b)[ok]
    rel = diff / np.maximum(np.abs(a[ok]), 1e-9)
    print(f"  {k:9s} max |diff| {diff.max():.3g}  max rel {rel.max():.3g}")
if "--detail" in sys.argv:
    for k, s in [("x_hg", "hg"), ("x_lg", "lg")]:
        bad = np.abs(ref[k] - erica[k]) > 1e-3
        print(f"{k}: {bad.sum()} rows differ > 1e-3; |{s}| of them (EricaNext): max {np.abs(erica[s][bad]).max() if bad.any() else 0:.3g}")
