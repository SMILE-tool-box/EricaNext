#!/usr/bin/env python3
"""Make the PSA temperature gain tables calib/psa/temp_gain_haNN.csv from calibration runs.

    /usr/bin/python3 make_psa_temp_gain.py cal_run*.root -o ../../calib/psa [--t-ref 44]

The gain of the MPPC changes with the temperature (about -6 %/degC, not linear), so PSA_Calibration
divides hg by the relative gain at the temperature of the event: hg_corr = hg / gain(T), T = mean of the
6 sensors of the board (temp_mean). This script makes gain(T) for every board and ch:

  1. hits at the trigger time (peak_id range from the data, as temp_spectra.py) without saturation
  2. hits divided into temperature bins of --temp-width degC (temp_mean)
  3. in every bin with at least --min-hits hits: photopeak of the hg spectrum (temp_spectra.py photopeak)
  4. gain = photopeak(T) / photopeak(T_ref), T_ref = --t-ref (linear between the bins), else the bin
     with the most hits of the ch

The inputs should cover the temperatures of the data to be corrected (outside the table the gain is the
value at the end). Use runs of one source (one line) so that the photopeak is the same line in every bin.
The hg of the inputs must be the raw hg (PSA_Calibration does not change hg; hg_corr is not used here).

Output: <out>/temp_gain_haNN.csv: comment lines, then ch,temp,gain,photopeak,hits (temp = bin center).
PSA_Calibration reads ch, temp, gain. Look at <out>/qa/temp_gain_haNN.png before using it.
Needs numpy, matplotlib and PyROOT: /usr/bin/python3.
"""
import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from psa_calib_common import header, on_time_range, photopeak, read_tree


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("root", nargs="+", help="outputs of run_chain.py smile3_psa of calibration runs")
    ap.add_argument("-o", "--out-dir", required=True)
    ap.add_argument("--boards", type=int, nargs="*", help="only these boards (default: all)")
    ap.add_argument("--temp-width", type=float, default=1.0, help="width of a temperature bin [degC]")
    ap.add_argument("--min-hits", type=int, default=5000, help="bins with fewer hits in the ch are not used")
    ap.add_argument("--t-ref", type=float, default=None, help="temperature where gain = 1 [degC]")
    ap.add_argument("--hg-max", type=float, default=100e3, help="upper edge of the spectrum [ADC x sample]")
    ap.add_argument("--hg-bins", type=int, default=250)
    ap.add_argument("--peak-min", type=float, default=15e3, help="search the photopeak above this")
    ap.add_argument("--peak-id-window", type=int, default=None,
                    help="hits with |peak_id - most frequent| <= this (default: from the data)")
    ap.add_argument("--no-qa", action="store_true")
    ap.add_argument("--overwrite", action="store_true")
    args = ap.parse_args()

    sel = "!satu_h && !satu_l"
    if args.boards:
        sel += " && (" + " || ".join(f"board == {b}" for b in args.boards) + ")"
    d = read_tree(args.root, ["board", "ch", "hg", "peak_id", "temp_mean"], sel)
    edges = np.linspace(0, args.hg_max, args.hg_bins + 1)
    centers = 0.5 * (edges[:-1] + edges[1:])
    os.makedirs(args.out_dir, exist_ok=True)

    for b in np.unique(d["board"]):
        out = os.path.join(args.out_dir, f"temp_gain_ha{b:02d}.csv")
        if os.path.exists(out) and not args.overwrite:
            print(f"ha{b:02d}: {out} exists, skipped (--overwrite to replace)")
            continue
        m = d["board"] == b
        lo, hi = on_time_range(d["peak_id"][m], args.peak_id_window)
        m &= (d["peak_id"] >= lo) & (d["peak_id"] <= hi)
        t = d["temp_mean"][m]
        w = args.temp_width
        lows = np.arange(np.floor(t.min() / w) * w, t.max(), w)
        rows, notes, spectra = [], [], {}
        for c in range(6):
            mc = m & (d["ch"] == c)
            pts = []
            for low in lows:
                s = mc & (d["temp_mean"] >= low) & (d["temp_mean"] < low + w)
                n = int(s.sum())
                if n < args.min_hits:
                    continue
                counts = np.histogram(d["hg"][s], bins=edges)[0]
                pk = photopeak(counts, centers, args.peak_min)
                if np.isfinite(pk):
                    pts.append((low + w / 2, pk, n))
                    spectra[(c, low + w / 2)] = counts / n
            if not pts:
                notes.append(f"ch{c}: no temperature bin with {args.min_hits} hits")
                continue
            tt, pp, nn = (np.array(v) for v in zip(*pts))
            ref = np.interp(args.t_ref, tt, pp) if args.t_ref is not None else pp[np.argmax(nn)]
            t_ref = args.t_ref if args.t_ref is not None else tt[np.argmax(nn)]
            rows += [(c, a, p / ref, p, n) for a, p, n in zip(tt, pp, nn)]
            notes.append(f"ch{c}: {len(pts)} bins {tt.min():.1f}-{tt.max():.1f} degC, gain = 1 at {t_ref:.1f} degC, "
                         f"gain {np.min(pp) / ref:.3f}-{np.max(pp) / ref:.3f}")
        if not rows:
            print(f"ha{b:02d}: nothing written")
            for s in notes:
                print("   ", s)
            continue
        with open(out, "w") as f:
            f.write(header("make_psa_temp_gain.py", args.root, [
                f"hits: {sel}, peak_id {lo}-{hi}; T = temp_mean, bins of {w:g} degC with >= {args.min_hits} hits",
                f"photopeak of hg above {args.peak_min:g} ({args.hg_bins} bins up to {args.hg_max:g})"] + notes))
            f.write("ch,temp,gain,photopeak,hits\n")
            for r in rows:
                f.write(f"{r[0]},{r[1]:.4g},{r[2]:.6g},{r[3]:.6g},{r[4]}\n")
        print(f"ha{b:02d}: {out}")
        for s in notes:
            print("   ", s)
        if not args.no_qa:
            import matplotlib
            matplotlib.use("Agg")
            import matplotlib.pyplot as plt
            os.makedirs(os.path.join(args.out_dir, "qa"), exist_ok=True)
            fig, axs = plt.subplots(2, 6, figsize=(30, 8))
            for c in range(6):
                for (cc, tc), sp in sorted(spectra.items()):
                    if cc == c:
                        axs[0, c].step(centers, sp, where="mid", label=f"{tc:.1f} degC")
                r = [x for x in rows if x[0] == c]
                if r:
                    axs[0, c].axvline(r[0][3], color="k", lw=0.3)
                    axs[1, c].plot([x[1] for x in r], [x[2] for x in r], "o-")
                axs[0, c].set_title(f"ha{b:02d} ch{c}: hg per temperature"); axs[0, c].legend(fontsize=6)
                axs[1, c].set_xlabel("temp_mean [degC]"); axs[1, c].set_ylabel("gain")
            fig.tight_layout()
            fig.savefig(os.path.join(args.out_dir, "qa", f"temp_gain_ha{b:02d}.png"), dpi=60)
            plt.close(fig)


if __name__ == "__main__":
    sys.exit(main())
