#!/usr/bin/env python3
"""Make the PSA position calibration calib/psa/position_haNN.csv from calibration runs.

    # 1. run the chain on the calibration runs (any number; the more hits the better)
    cd run_script
    python3 run_chain.py smile3_psa -o cal_run01.root --data-dir <run01> --set 'PSA_ReadData.extra_data_dirs=["<run01 of the other storage>"]'
    python3 run_chain.py smile3_psa -o cal_run02.root --data-dir <run02> ...
    # 2. find the 64 peaks of every board and ch in the sum of them, and write the calibration
    /usr/bin/python3 ../scripts/calib/make_psa_position.py cal_run*.root -o ../calib/psa

For every board and ch, the x_hg-y_hg map (flood map) of the hits that pass the cuts is summed over the
input files, the 64 peaks are found, and pixel numbers (n, m) are given to them as grid_spect.py does
(sorted by x into 8 columns of 8, each column sorted by y). PSA_Calibration reads the file and maps
(x_hg, y_hg) -> (n, m) by the thin-plate spline through the 64 points; PSA_Event then converts the
pixel to detector coordinates [mm] with calib/psa/geometry.csv.

The peak finder is the one of peak_search.py (analysis_sm3_FM/PSA_analysis): the 64 highest local maxima
of the box-smoothed map, at least min_dist bins apart. It has no grid constraint, so with few hits it can
miss an outer column or take two maxima in one cluster; look at the QA figures (<out>/qa/) before using
the file, and remove a ch (or a board file) whose peaks are not an 8x8 grid: PSA_Calibration then gives
no pixel for it.

Output, per board: <out>/position_haNN.csv (comment lines '# ...' with the inputs and cuts, then
ch,No,x,y,z,n,m; z = counts at the peak) and <out>/qa/position_haNN.png (map + peaks, corrected map).
Boards with fewer than --min-hits hits in a ch are skipped for that ch.
Needs numpy, scipy, matplotlib and PyROOT: /usr/bin/python3 on the analysis machine.
"""
import argparse
import datetime
import os
import sys

import numpy as np

N_GRID = 8
NPEAKS = N_GRID * N_GRID
X_RANGE = (-0.32, 0.32)
Y_RANGE = (-0.16, 0.16)


def box_smooth(a, wx, wy):
    """running mean over (2 wx + 1) x (2 wy + 1) bins, reflected at the edges (peak_search.py box_smooth_2d)"""
    def smooth_1d(v, w, axis):
        if w <= 0:
            return v
        k = 2 * w + 1
        pad = [(0, 0), (0, 0)]
        pad[axis] = (w, w)
        vp = np.pad(v, pad, mode="reflect")
        cs = np.cumsum(vp, axis=axis, dtype=np.float64)
        zero = np.zeros_like(np.take(cs, [0], axis=axis))
        cs = np.concatenate([zero, cs], axis=axis)
        hi = np.take(cs, np.arange(k, cs.shape[axis]), axis=axis)
        lo = np.take(cs, np.arange(0, cs.shape[axis] - k), axis=axis)
        return (hi - lo) / float(k)
    return smooth_1d(smooth_1d(a, wy, 1), wx, 0)


def find_peaks(counts, x_edges, y_edges, npeaks=NPEAKS, thr_rel=0.01, smooth=(3, 3), min_dist=3):
    """peak_search.py find_peaks_localmax: [(x, y, counts at the peak)], highest first"""
    s = box_smooth(counts, *smooth) if (smooth[0] > 0 or smooth[1] > 0) else counts
    vmax = float(s.max())
    if vmax <= 0:
        return []
    p = np.pad(s, 1, mode="constant", constant_values=-np.inf)
    c = p[1:-1, 1:-1]
    m = c >= thr_rel * vmax
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            if dx or dy:
                m &= c >= p[1 + dx:p.shape[0] - 1 + dx, 1 + dy:p.shape[1] - 1 + dy]
    idx = np.argwhere(m)
    order = np.argsort(s[idx[:, 0], idx[:, 1]])[::-1]
    xc = 0.5 * (x_edges[:-1] + x_edges[1:])
    yc = 0.5 * (y_edges[:-1] + y_edges[1:])
    kept, peaks = [], []
    for k in order:
        ix, iy = int(idx[k, 0]), int(idx[k, 1])
        if min_dist > 0 and any((ix - jx) ** 2 + (iy - jy) ** 2 < min_dist ** 2 for jx, jy in kept):
            continue
        peaks.append((float(xc[ix]), float(yc[iy]), float(counts[ix, iy])))
        kept.append((ix, iy))
        if len(peaks) >= npeaks:
            break
    return peaks


def assign_nm(x, y):
    """grid_spect.py: 8 columns of 8 by x (numpy argsort), each column sorted by y. Same as run_script/psa_position_nm.py"""
    n = np.empty(len(x), dtype=int)
    m = np.empty(len(x), dtype=int)
    for group, idx in enumerate(np.array_split(np.argsort(x), N_GRID), start=1):
        n[idx] = group
        m[idx[np.argsort(y[idx])]] = np.arange(1, N_GRID + 1)
    return n, m


def read_hits(files, args):
    """{board: {ch: (x_hg, y_hg)}} of the hits that pass the cuts, from PSA_TreeWriter/psa of all files"""
    import ROOT
    cut = [f"hg >= {args.hg_min}", f"hg <= {args.hg_max}", f"peak_id >= {args.peak_id_min}", f"peak_id <= {args.peak_id_max}"]
    if args.skip_saturated:
        cut.append("!satu_h && !satu_l")
    if args.boards:
        cut.append("(" + " || ".join(f"board == {b}" for b in args.boards) + ")")
    sel = " && ".join(cut)
    chain = ROOT.TChain("PSA_TreeWriter/psa")
    for f in files:
        chain.Add(f)
    d = ROOT.RDataFrame(chain).Filter(sel).AsNumpy(["board", "ch", "x_hg", "y_hg"])
    board, ch = d["board"].astype(int), d["ch"].astype(int)
    hits = {}
    for b in np.unique(board):
        hits[b] = {c: (d["x_hg"][(board == b) & (ch == c)], d["y_hg"][(board == b) & (ch == c)]) for c in range(6)}
    return hits, sel


def qa_figure(path, b, maps, peaks, edges, frac, corrected):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.colors import LogNorm
    fig, axs = plt.subplots(2, 6, figsize=(30, 8))
    for c in range(6):
        ax = axs[0, c]
        if maps[c] is not None and maps[c].sum() > 0:
            ax.pcolormesh(edges[0], edges[1], np.ma.masked_equal(maps[c].T, 0), norm=LogNorm(vmin=1), shading="flat")
        for (x, y, _), n, m in peaks.get(c, []):
            ax.plot(x, y, "rx", ms=4)
            ax.text(x, y, f"{n}{m}", color="w", fontsize=6)
        ax.set_title(f"ha{b:02d} ch{c}: x_hg-y_hg, 64 peaks (n m)")
        ax = axs[1, c]
        if c in corrected:
            xc, yc = corrected[c]
            ax.hist2d(xc, yc, bins=180, range=[[-0.5, 9.5], [-0.5, 9.5]], norm=LogNorm())
            for v in np.arange(0.5, 9):
                ax.axvline(v, color="r", lw=0.4)
                ax.axhline(v, color="r", lw=0.4)
            ax.set_title(f"corrected (n, m): {100 * frac[c]:.1f} % in 8x8")
        ax.set_aspect("equal")
    fig.tight_layout()
    fig.savefig(path, dpi=60)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("root", nargs="+", help="outputs of run_chain.py smile3_psa (PSA_TreeWriter/psa) of calibration runs")
    ap.add_argument("-o", "--out-dir", required=True, help="where position_haNN.csv are written (e.g. ../calib/psa)")
    ap.add_argument("--boards", type=int, nargs="*", help="only these boards (default: all)")
    ap.add_argument("--hg-min", type=float, default=30e3, help="hits with hg >= this [ADC x sample] (peak_search.py --emin)")
    ap.add_argument("--hg-max", type=float, default=500e3, help="hits with hg <= this (peak_search.py --emax)")
    ap.add_argument("--peak-id-min", type=int, default=0, help="hits with peak_id >= this")
    ap.add_argument("--peak-id-max", type=int, default=49, help="hits with peak_id <= this")
    ap.add_argument("--skip-saturated", action="store_true", help="drop hits with satu_h or satu_l")
    ap.add_argument("--bins", type=int, nargs=2, default=[250, 300], help="bins of the map in x, y (peak_search.py: 250 300)")
    ap.add_argument("--smooth", type=int, nargs=2, default=[3, 3], help="half widths of the box smoothing [bins]")
    ap.add_argument("--thr-rel", type=float, default=0.01, help="local maxima above this fraction of the highest")
    ap.add_argument("--min-dist", type=int, default=3, help="peaks at least this far apart [bins]")
    ap.add_argument("--min-hits", type=int, default=10000, help="skip a ch with fewer hits")
    ap.add_argument("--no-qa", action="store_true", help="do not write the QA figures")
    ap.add_argument("--overwrite", action="store_true", help="overwrite existing position_haNN.csv")
    args = ap.parse_args()

    hits, sel = read_hits(args.root, args)
    x_edges = np.linspace(*X_RANGE, args.bins[0] + 1)
    y_edges = np.linspace(*Y_RANGE, args.bins[1] + 1)
    os.makedirs(args.out_dir, exist_ok=True)
    if not args.no_qa:
        os.makedirs(os.path.join(args.out_dir, "qa"), exist_ok=True)
    now = datetime.datetime.now().isoformat(timespec="seconds")

    for b, per_ch in sorted(hits.items()):
        out = os.path.join(args.out_dir, f"position_ha{b:02d}.csv")
        if os.path.exists(out) and not args.overwrite:
            print(f"ha{b:02d}: {out} exists, skipped (--overwrite to replace)")
            continue
        maps, peaks, rows, frac, corrected, notes = [None] * 6, {}, [], {}, {}, []
        for c in range(6):
            x, y = per_ch[c]
            if len(x) < args.min_hits:
                notes.append(f"ch{c}: {len(x)} hits < --min-hits {args.min_hits}, no peaks")
                continue
            maps[c] = np.histogram2d(x, y, bins=[x_edges, y_edges])[0]
            pk = find_peaks(maps[c], x_edges, y_edges, NPEAKS, args.thr_rel, tuple(args.smooth), args.min_dist)
            if len(pk) != NPEAKS:
                notes.append(f"ch{c}: {len(pk)} peaks found, expected {NPEAKS}; ch not written")
                continue
            px, py = np.array([p[0] for p in pk]), np.array([p[1] for p in pk])
            n, m = assign_nm(px, py)
            peaks[c] = list(zip(pk, n, m))
            rows += [(c, k + 1, p[0], p[1], p[2], n[k], m[k]) for k, p in enumerate(pk)]
            from scipy.interpolate import RBFInterpolator
            w = RBFInterpolator(np.column_stack([px, py]), np.column_stack([n, m]).astype(float),
                                kernel="thin_plate_spline", smoothing=0)
            cxy = w(np.column_stack([x, y]))
            inside = np.all((np.floor(cxy + 0.5) >= 1) & (np.floor(cxy + 0.5) <= N_GRID), axis=1)
            frac[c] = float(inside.mean())
            corrected[c] = (cxy[:, 0], cxy[:, 1])
            notes.append(f"ch{c}: {len(x)} hits, {100 * frac[c]:.1f} % of them in 8x8 after the correction")
        if not rows:
            print(f"ha{b:02d}: no ch with 64 peaks; nothing written")
            for s in notes:
                print("   ", s)
            continue
        with open(out, "w") as w:
            w.write(f"# made by scripts/calib/make_psa_position.py, {now}\n")
            for f in args.root:
                w.write(f"# input: {os.path.abspath(f)}\n")
            w.write(f"# hits: {sel}\n")
            w.write(f"# map: bins {args.bins[0]} x {args.bins[1]} in x {X_RANGE}, y {Y_RANGE}; smooth {args.smooth}, "
                    f"thr_rel {args.thr_rel}, min_dist {args.min_dist}\n")
            for s in notes:
                w.write(f"# {s}\n")
            w.write("ch,No,x,y,z,n,m\n")
            for r in rows:
                w.write(f"{r[0]},{r[1]},{r[2]:.10g},{r[3]:.10g},{r[4]:.10g},{r[5]},{r[6]}\n")
        print(f"ha{b:02d}: {out}")
        for s in notes:
            print("   ", s)
        if not args.no_qa:
            qa_figure(os.path.join(args.out_dir, "qa", f"position_ha{b:02d}.png"), b, maps, peaks,
                      (x_edges, y_edges), frac, corrected)


if __name__ == "__main__":
    sys.exit(main())
