#!/usr/bin/env python3
"""TPC drift velocity calibration file (calib/tpc/drift_velocity.dat) from the raw data tree.

    python3 run_script/run_chain.py smile3_raw -o raw.root --data-dir <run> --compression 505
    /usr/bin/python3 scripts/calib/make_tpc_drift_velocity.py -o calib/tpc/drift_velocity.dat raw.root [raw2.root ...]

Method (EricaNext_try tutorials_caldata/4_MakeCalibration/DriftVelocity, drift_speed_s2p_v3.C):
  1. clock distribution of all strip hits (hit_a_clock / hit_c_clock of RawTreeWriter_sm3/raw), anode and
     cathode separately. Electrons made uniformly between the GEM plane and the top plate give a box.
  2. derivative |h[b+1] - h[b]|: the two edges of the box become two peaks.
  3. clock_offset: the clock of the largest derivative in --gem-range (GEM side edge).
  4. top side edge: Gaussian fit of the derivative peak in --top-range (initial values: maximum, weighted mean
     and RMS of the derivative there; fit range mean +- 1.5 RMS).
  5. velocity = (drift_length - z_gem) / (top edge - clock_offset).
  clock_offset and velocity are the means of anode and cathode.
SMILE-3: z_gem = 10 mm (uPIC - GEM), drift length = 317 mm (uPIC - top plate), so 307 mm between the edges.

The run must have the electrons uniformly in the drift volume (the clock distribution must be a box). The
file is written only when the checks pass (--force writes it anyway); look at <output dir>/qa/<name>.png.
Unlike SMILE-2+ (velocity vs time in 10 min bins), the SMILE-3 file has one value for the whole run;
TOT_Skewness reads the lines "clock_offset <clock>" and "velocity <mm/clock>".
"""
import argparse
import datetime
import os
import sys

import numpy as np
import ROOT

ROOT.gROOT.SetBatch(True)


def clock_hist(paths, column, nclk, tree):
    h = None
    for p in paths:
        df = ROOT.RDataFrame(tree, p)
        x = df.Histo1D(("h_" + column, "", nclk, -0.5, nclk - 0.5), column).GetValue()
        if h is None:
            h = x.Clone(); h.SetDirectory(0)
        else:
            h.Add(x)
    return h


def derivative(y):
    """|h[b+1] - h[b]|, stored at b (as EricaNext_try differential())"""
    d = np.zeros_like(y)
    d[:-1] = np.abs(np.diff(y))
    return d


def analyze(h, gem_range, top_range, length):
    n = h.GetNbinsX()
    clk = np.array([h.GetBinCenter(i) for i in range(1, n + 1)])
    y = np.array([h.GetBinContent(i) for i in range(1, n + 1)])
    d = derivative(y)
    r = {"clk": clk, "y": y, "d": d, "problems": []}

    g = (clk >= gem_range[0]) & (clk <= gem_range[1])
    r["offset"] = float(clk[g][np.argmax(d[g])])

    t = (clk >= top_range[0]) & (clk <= top_range[1])
    w = d[t]
    if w.sum() <= 0:
        r["problems"].append("no hits in the top range")
        return r
    mean = float((clk[t] * w).sum() / w.sum())
    rms = float(np.sqrt(max((clk[t] ** 2 * w).sum() / w.sum() - mean ** 2, 1e-6)))
    lo, hi = max(mean - 1.5 * rms, top_range[0]), min(mean + 1.5 * rms, top_range[1])
    hd = ROOT.TH1D("hd", "", n, clk[0] - 0.5, clk[-1] + 0.5); hd.SetDirectory(0)
    for i, v in enumerate(d):
        hd.SetBinContent(i + 1, v)
    f = ROOT.TF1("fg", "gaus", lo, hi)
    f.SetParameters(float(w.max()), mean, rms)
    st = int(hd.Fit(f, "QN0", "", lo, hi))
    edge, err, sigma, height = f.GetParameter(1), f.GetParError(1), abs(f.GetParameter(2)), f.GetParameter(0)
    r.update(edge=edge, edge_err=err, sigma=sigma, height=height, fit_range=(lo, hi))
    r["velocity"] = length / (edge - r["offset"]) if edge > r["offset"] else float("nan")

    # checks: the fit converged inside the range, the edge is sharp, and the box is there
    plateau = (clk > r["offset"] + 20) & (clk < edge - 20)
    noise = float(np.median(d[plateau])) if plateau.sum() > 10 else float("nan")
    r["plateau_noise"] = noise
    if st != 0:
        r["problems"].append(f"top edge fit status {st}")
    if not (top_range[0] < edge < top_range[1]):
        r["problems"].append(f"top edge {edge:.1f} outside --top-range")
    if not (err < 5):
        r["problems"].append(f"top edge error {err:.1f} clock (>= 5)")
    if not (sigma < 30):
        r["problems"].append(f"top edge width {sigma:.1f} clock (>= 30): not a sharp edge")
    if not (height > 5 * noise):
        r["problems"].append(f"top edge peak {height:.0f} not above 5 x the plateau fluctuation {noise:.0f}")
    if plateau.sum() > 10:
        ym = y[plateau]
        ratio = ym[: len(ym) // 4].mean() / max(ym[-len(ym) // 4:].mean(), 1)
        r["box_ratio"] = ratio
        if not (0.33 < ratio < 3):
            r["problems"].append(f"not a box: the first quarter / last quarter of the plateau = {ratio:.1f}")
    return r


def qa_figure(path, res):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(2, 2, figsize=(13, 7), sharex=True)
    for col, (side, r) in enumerate(res.items()):
        ax[0, col].step(r["clk"], r["y"], where="mid", lw=0.8)
        ax[0, col].set_title(f"{side}: clock distribution of the hits"); ax[0, col].set_ylabel("hits")
        ax[1, col].step(r["clk"], np.maximum(r["d"], 0.5), where="mid", lw=0.8)
        ax[1, col].set_yscale("log"); ax[1, col].set_ylabel("|h[b+1] - h[b]|"); ax[1, col].set_xlabel("clock")
        for a in ax[:, col]:
            a.axvline(r["offset"], color="r", ls="--", lw=0.8, label=f"clock_offset {r['offset']:.0f}")
            if "edge" in r and r["clk"][0] <= r["edge"] <= r["clk"][-1]:
                a.axvline(r["edge"], color="g", ls="--", lw=0.8, label=f"top edge {r['edge']:.1f}")
        if "fit_range" in r:
            x = np.linspace(*r["fit_range"], 200)
            ax[1, col].plot(x, r["height"] * np.exp(-0.5 * ((x - r["edge"]) / r["sigma"]) ** 2), "g-", lw=1)
        ax[1, col].set_ylim(bottom=0.5); ax[1, col].set_xlim(r["clk"][0], r["clk"][-1])
        ax[0, col].legend(fontsize=8)
        txt = f"v = {r.get('velocity', float('nan')):.4f} mm/clock"
        if r["problems"]:
            txt += "\nNG: " + "\n".join(r["problems"])
        ax[1, col].text(0.02, 0.03, txt, transform=ax[1, col].transAxes, fontsize=8, va="bottom",
                        color="r" if r["problems"] else "k")
    fig.tight_layout()
    fig.savefig(path, dpi=70)
    print("wrote", path)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="+", help="output ROOT files of smile3_raw (RawTreeWriter_sm3/raw)")
    ap.add_argument("-o", "--output", required=True, help="output file (e.g. calib/tpc/drift_velocity.dat)")
    ap.add_argument("--z-gem", type=float, default=10.0, help="uPIC - GEM [mm] (SMILE-3 10)")
    ap.add_argument("--drift-length", type=float, default=317.0, help="uPIC - top plate [mm] (SMILE-3 317)")
    ap.add_argument("--gem-range", type=float, nargs=2, default=(0, 300), help="clocks to look for the GEM side edge")
    ap.add_argument("--top-range", type=float, nargs=2, default=(400, 1023), help="clocks to look for the top side edge")
    ap.add_argument("--nclk", type=int, default=1024, help="clocks of the encoder")
    ap.add_argument("--tree", default="RawTreeWriter_sm3/raw", help="raw data tree (SMILE-2+: RawTreeWriter_2p/raw)")
    ap.add_argument("--overwrite", action="store_true")
    ap.add_argument("--force", action="store_true", help="write the file even if a check fails")
    ap.add_argument("--no-qa", action="store_true")
    args = ap.parse_args()

    if os.path.exists(args.output) and not args.overwrite:
        sys.exit(f"{args.output} exists (--overwrite to replace it)")
    length = args.drift_length - args.z_gem
    res = {}
    for side, col in (("anode", "hit_a_clock"), ("cathode", "hit_c_clock")):
        r = analyze(clock_hist(args.inputs, col, args.nclk, args.tree), args.gem_range, args.top_range, length)
        res[side] = r
        print(f"{side}: {int(r['y'].sum())} hits, clock_offset {r['offset']:.0f}, top edge "
              f"{r.get('edge', float('nan')):.2f} +- {r.get('edge_err', float('nan')):.2f} (sigma {r.get('sigma', float('nan')):.1f}), "
              f"velocity {r.get('velocity', float('nan')):.5f} mm/clock")
        for p in r["problems"]:
            print(f"  NG: {p}")

    if not args.no_qa:
        qa = os.path.join(os.path.dirname(os.path.abspath(args.output)), "qa")
        os.makedirs(qa, exist_ok=True)
        qa_figure(os.path.join(qa, os.path.splitext(os.path.basename(args.output))[0] + ".png"), res)

    bad = any(r["problems"] or not np.isfinite(r.get("velocity", float("nan"))) for r in res.values())
    if bad and not args.force:
        sys.exit("checks failed: the file is not written (--force to write it anyway). "
                 "The run needs electrons uniformly in the drift volume.")
    offset = np.mean([r["offset"] for r in res.values()])
    v = np.mean([r["velocity"] for r in res.values()])
    with open(args.output, "w") as f:
        f.write(f"# made by scripts/calib/make_tpc_drift_velocity.py, {datetime.datetime.now().isoformat(timespec='seconds')}\n")
        for p in args.inputs:
            f.write(f"# input: {os.path.abspath(p)}\n")
        f.write(f"# z_gem {args.z_gem} mm, drift length {args.drift_length} mm: {length} mm between the edges\n")
        for side, r in res.items():
            f.write(f"# {side}: clock_offset {r['offset']:.0f}, top edge {r['edge']:.2f} +- {r['edge_err']:.2f}, "
                    f"velocity {r['velocity']:.5f}{' (NG: ' + '; '.join(r['problems']) + ')' if r['problems'] else ''}\n")
        f.write(f"clock_offset {offset:g}\nvelocity {v:.5f}\n")
    print(f"wrote {args.output}: clock_offset {offset:g}, velocity {v:.5f} mm/clock")


if __name__ == "__main__":
    main()
