#!/usr/bin/env python3
"""Add the pixel numbers n, m (1-8) to position_haNN.csv of peak_search.py, for PSA_Calibration.

    python3 psa_position_nm.py position_ha03.csv [position_ha04.csv ...] -o ../calib/psa

The 64 peaks of each ch are given (n, m) exactly as grid_spect.py does: sorted by x
(numpy argsort) into 8 columns of 8, each column sorted by y. The peaks of peak_search.py
are at bin centers, so equal x or y happen; numpy then decides the order, and this script
writes that decision into the file so that PSA_Calibration (C++) uses the same (n, m) as
grid_spect.py and pixel_id.py. Check or fix n, m by hand in the output when needed.
Output: <out-dir>/<same name>, columns ch,No,x,y,z,n,m.
"""
import argparse
import os

import numpy as np

N_GRID = 8


def assign(x, y):
    # grid_spect.py
    n = np.empty(len(x), dtype=int)
    m = np.empty(len(x), dtype=int)
    for group, idx in enumerate(np.array_split(np.argsort(x), N_GRID), start=1):
        n[idx] = group
        m[idx[np.argsort(y[idx])]] = np.arange(1, N_GRID + 1)
    return n, m


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", nargs="+", help="position_haNN.csv of peak_search.py")
    ap.add_argument("-o", "--out-dir", required=True)
    args = ap.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)

    for path in args.csv:
        d = np.genfromtxt(path, delimiter=",", names=True, dtype=None, encoding="utf-8")
        n = np.zeros(len(d), dtype=int)
        m = np.zeros(len(d), dtype=int)
        for ch in np.unique(d["ch"]):
            sel = np.where(d["ch"] == ch)[0]
            if len(sel) != N_GRID * N_GRID:
                raise SystemExit(f"{path}: ch{ch} has {len(sel)} points, expected 64")
            n[sel], m[sel] = assign(d["x"][sel].astype(float), d["y"][sel].astype(float))
        out = os.path.join(args.out_dir, os.path.basename(path))
        if os.path.abspath(out) == os.path.abspath(path):
            raise SystemExit(f"{out}: would overwrite the input")
        with open(out, "w") as w:
            w.write("ch,No,x,y,z,n,m\n")
            for r, nn, mm in zip(d, n, m):
                w.write(f"{r['ch']},{r['No']},{r['x']:.10g},{r['y']:.10g},{r['z']:.10g},{nn},{mm}\n")
        print(f"{path} -> {out}")


if __name__ == "__main__":
    main()
