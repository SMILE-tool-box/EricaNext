#!/usr/bin/env python3
"""TPC drift velocity calibration file (calib/tpc/drift_velocity.dat) from the raw data tree.

    python3 run_script/run_chain.py smile3_raw -o raw.root --data-dir <run> --compression 505
    /usr/bin/python3 scripts/calib/make_tpc_drift_velocity.py -o calib/tpc/drift_velocity.dat raw.root [raw2.root ...]

Method (EricaNext_try tutorials_caldata/4_MakeCalibration/DriftVelocity, drift_speed_s2p_v3.C):
  1. clock distribution of all strip hits (hit_a_clock / hit_c_clock of RawTreeWriter_sm3/raw), anode and
     cathode separately. Electrons made uniformly between the GEM plane and the top plate give a box.
     --rebin N sums N clocks into one bin first (for runs with few hits, where the derivative is noisy).
  2. derivative |h[b+1] - h[b]| (first derivative): the two edges of the box become two peaks.
     The steps from the first bin and into the last bin (the ends of the encoder range, which often collect
     hits) are set to 0, so that they are never taken as an edge.
  3. clock_offset (GEM side edge): Gaussian fit of the derivative peak in --gem-range, fit range the clock
     of the largest derivative +- --gem-window (default 40; the GEM side peak is only a few times above the
     plateau fluctuation, so a wide range picks up the noise).
     top side edge: Gaussian fit of the derivative peak in --top-range (initial values: maximum, weighted
     mean and RMS of the derivative there; fit range mean +- 1.5 RMS).
     Until 2026-11-13 clock_offset was the clock of the largest derivative (no fit; as
     EricaNext_try), which jumps by the noise when the GEM side edge is not sharp.
     Default ranges: --gem-range 20 300, --top-range 800 1000 (SMILE-3 2026-11-11 run70: edges at about 100
     and 890 clock). Look at the clock distribution and set them for the run.
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
    d[0] = 0.0    # step from the first bin (end of the encoder range)
    d[-2] = 0.0   # step into the last bin (end of the encoder range)
    return d


def fit_edge(clk, d, rng, half_window=None):
    """Gaussian fit of the derivative peak in rng. Returns None if the range has no derivative.
    half_window None: initial values maximum, weighted mean and RMS of the derivative in rng, fit range
    mean +- 1.5 RMS (a peak far above the rest of the range). half_window W [clock]: initial values maximum,
    its clock and W / 2, fit range that clock +- W (a peak only a few times above the noise)."""
    sel = (clk >= rng[0]) & (clk <= rng[1])
    w = d[sel]
    if w.sum() <= 0:
        return None
    bw = clk[1] - clk[0]
    if half_window is None:
        mean = float((clk[sel] * w).sum() / w.sum())
        rms = float(np.sqrt(max((clk[sel] ** 2 * w).sum() / w.sum() - mean ** 2, 1e-6)))
        lo, hi = max(mean - 1.5 * rms, rng[0]), min(mean + 1.5 * rms, rng[1])
    else:
        hw = max(half_window, 2 * bw)
        mean, rms = float(clk[sel][np.argmax(w)]), hw / 2
        lo, hi = max(mean - hw, rng[0]), min(mean + hw, rng[1])
    hd = ROOT.TH1D("hd", "", len(clk), clk[0] - bw / 2, clk[-1] + bw / 2); hd.SetDirectory(0)
    for i, v in enumerate(d):
        hd.SetBinContent(i + 1, v)
    f = ROOT.TF1("fg", "gaus", lo, hi)
    f.SetParameters(float(w.max()), mean, rms)
    st = int(hd.Fit(f, "QN0", "", lo, hi))
    return {"pos": f.GetParameter(1), "err": f.GetParError(1), "sigma": abs(f.GetParameter(2)),
            "height": f.GetParameter(0), "fit_range": (lo, hi), "status": st}


def analyze(h, gem_range, top_range, length, gem_window=40.0):
    n = h.GetNbinsX()
    clk = np.array([h.GetBinCenter(i) for i in range(1, n + 1)])
    y = np.array([h.GetBinContent(i) for i in range(1, n + 1)])
    d = derivative(y)
    r = {"clk": clk, "y": y, "d": d, "problems": []}

    fits = {}
    for name, rng, hw in (("GEM", gem_range, gem_window), ("top", top_range, None)):
        fits[name] = fit_edge(clk, d, rng, hw)
        if fits[name] is None:
            r["problems"].append(f"no hits in the {name} range")
    if None in fits.values():
        return r
    g, t = fits["GEM"], fits["top"]
    r.update(offset=g["pos"], offset_err=g["err"], gem_sigma=g["sigma"], gem_height=g["height"],
             gem_fit_range=g["fit_range"],
             edge=t["pos"], edge_err=t["err"], sigma=t["sigma"], height=t["height"], fit_range=t["fit_range"])
    edge = t["pos"]
    r["velocity"] = length / (edge - r["offset"]) if edge > r["offset"] else float("nan")

    # checks: the fits converged inside the ranges, the edges are sharp, and the box is there
    plateau = (clk > r["offset"] + 20) & (clk < edge - 20)
    noise = float(np.median(d[plateau])) if plateau.sum() > 10 else float("nan")
    r["plateau_noise"] = noise
    for name, rng, e in (("GEM", gem_range, g), ("top", top_range, t)):
        if e["status"] != 0:
            r["problems"].append(f"{name} edge fit status {e['status']}")
        if not (rng[0] < e["pos"] < rng[1]):
            r["problems"].append(f"{name} edge {e['pos']:.1f} outside --{name.lower()}-range")
        if not (e["err"] < 5):
            r["problems"].append(f"{name} edge error {e['err']:.1f} clock (>= 5)")
        if not (e["sigma"] < 30):
            r["problems"].append(f"{name} edge width {e['sigma']:.1f} clock (>= 30): not a sharp edge")
        if not (e["height"] > 5 * noise):
            r["problems"].append(f"{name} edge peak {e['height']:.0f} not above 5 x the plateau fluctuation {noise:.0f}")
    if plateau.sum() > 10:
        ym = y[plateau]
        ratio = ym[: len(ym) // 4].mean() / max(ym[-len(ym) // 4:].mean(), 1)
        r["box_ratio"] = ratio
        if not (0.33 < ratio < 3):
            r["problems"].append(f"not a box: the first quarter / last quarter of the plateau = {ratio:.1f}")
    return r


def qa_figure(path, res):
    # matplotlib.figure.Figure draws to the file without pyplot, so the backend is not changed
    # (matplotlib.use("Agg") here stopped plt.show() in a notebook that imports this function)
    from matplotlib.figure import Figure
    fig = Figure(figsize=(13, 7))
    ax = fig.subplots(2, 2, sharex=True)
    for col, (side, r) in enumerate(res.items()):
        ax[0, col].step(r["clk"], r["y"], where="mid", lw=0.8)
        ax[0, col].set_title(f"{side}: clock distribution of the hits"); ax[0, col].set_ylabel("hits")
        ax[1, col].step(r["clk"], np.maximum(r["d"], 0.5), where="mid", lw=0.8)
        ax[1, col].set_yscale("log"); ax[1, col].set_ylabel("|h[b+1] - h[b]|"); ax[1, col].set_xlabel("clock")
        for a in ax[:, col]:
            if "offset" in r:
                a.axvline(r["offset"], color="r", ls="--", lw=0.8, label=f"clock_offset {r['offset']:.1f}")
            if "edge" in r and r["clk"][0] <= r["edge"] <= r["clk"][-1]:
                a.axvline(r["edge"], color="g", ls="--", lw=0.8, label=f"top edge {r['edge']:.1f}")
        for fr, hgt, pos, sg, c in (("gem_fit_range", "gem_height", "offset", "gem_sigma", "r"),
                                    ("fit_range", "height", "edge", "sigma", "g")):
            if fr in r:
                x = np.linspace(*r[fr], 200)
                ax[1, col].plot(x, r[hgt] * np.exp(-0.5 * ((x - r[pos]) / r[sg]) ** 2), c + "-", lw=1)
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
    ap.add_argument("--gem-range", type=float, nargs=2, default=(20, 300), help="clocks to look for the GEM side edge")
    ap.add_argument("--top-range", type=float, nargs=2, default=(800, 1000), help="clocks to look for the top side edge")
    ap.add_argument("--nclk", type=int, default=1024, help="clocks of the encoder")
    ap.add_argument("--gem-window", type=float, default=40.0, help="half width of the GEM side fit range [clock]")
    ap.add_argument("--rebin", type=int, default=1, help="clocks summed into one bin (must divide --nclk)")
    ap.add_argument("--tree", default="RawTreeWriter_sm3/raw", help="raw data tree (SMILE-2+: RawTreeWriter_2p/raw)")
    ap.add_argument("--overwrite", action="store_true")
    ap.add_argument("--force", action="store_true", help="write the file even if a check fails")
    ap.add_argument("--no-qa", action="store_true")
    args = ap.parse_args()

    if args.rebin < 1 or args.nclk % args.rebin:
        sys.exit(f"--rebin {args.rebin} must be >= 1 and divide --nclk {args.nclk}")
    if os.path.exists(args.output) and not args.overwrite:
        sys.exit(f"{args.output} exists (--overwrite to replace it)")
    length = args.drift_length - args.z_gem
    res = {}
    for side, col in (("anode", "hit_a_clock"), ("cathode", "hit_c_clock")):
        h = clock_hist(args.inputs, col, args.nclk, args.tree)
        if args.rebin > 1:
            h.Rebin(args.rebin)
        r = analyze(h, args.gem_range, args.top_range, length, args.gem_window)
        res[side] = r
        print(f"{side}: {int(r['y'].sum())} hits, clock_offset {r.get('offset', float('nan')):.2f} +- "
              f"{r.get('offset_err', float('nan')):.2f}, top edge "
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
    if any("offset" not in r or "edge" not in r for r in res.values()):
        sys.exit("an edge was not found: the file is not written")
    offset = np.mean([r["offset"] for r in res.values()])
    v = np.mean([r["velocity"] for r in res.values()])
    with open(args.output, "w") as f:
        f.write(f"# made by scripts/calib/make_tpc_drift_velocity.py, {datetime.datetime.now().isoformat(timespec='seconds')}\n")
        for p in args.inputs:
            f.write(f"# input: {os.path.abspath(p)}\n")
        f.write(f"# gem range {args.gem_range[0]:g}-{args.gem_range[1]:g} (fit +- {args.gem_window:g}), top range {args.top_range[0]:g}-{args.top_range[1]:g} clock, rebin {args.rebin}\n")
        f.write(f"# z_gem {args.z_gem} mm, drift length {args.drift_length} mm: {length} mm between the edges\n")
        for side, r in res.items():
            f.write(f"# {side}: clock_offset {r['offset']:.2f} +- {r['offset_err']:.2f}, top edge {r['edge']:.2f} +- {r['edge_err']:.2f}, "
                    f"velocity {r['velocity']:.5f}{' (NG: ' + '; '.join(r['problems']) + ')' if r['problems'] else ''}\n")
        f.write(f"clock_offset {offset:.2f}\nvelocity {v:.5f}\n")
    print(f"wrote {args.output}: clock_offset {offset:.2f}, velocity {v:.5f} mm/clock")


if __name__ == "__main__":
    main()
