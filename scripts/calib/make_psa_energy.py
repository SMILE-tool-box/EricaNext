#!/usr/bin/env python3
"""Make the PSA energy calibration calib/psa/energy_haNN.csv from runs of a known source.

    /usr/bin/python3 make_psa_energy.py src_run*.root -o ../../calib/psa --line 662
    /usr/bin/python3 make_psa_energy.py src_run*.root -o ../../calib/psa --line 356:30e3:60e3 --line 81:5e3:15e3

The inputs must be made with the position calibration (pixel) and the temperature gain table in place
(make_psa_position.py, make_psa_temp_gain.py), because the spectra are of hg_corr = hg / gain(T) per
pixel. PSA_Calibration then gives energy = p0 + p1 hg_corr [keV].

For every pixel (board, ch, n, m) and every line:
  - the hg_corr spectrum of the hits at the trigger time without saturation,
  - the line position: --line E (keV) alone = photopeak above --peak-min (temp_spectra.py photopeak);
    --line E:lo:hi = the maximum of the smoothed spectrum between lo and hi (use this for 2 or more lines),
  - one line: p0 = 0, p1 = E / position; two or more: least-squares line E = p0 + p1 position.
A pixel with fewer than --min-hits hits uses the sum of its ch (column src = "ch"), so that every pixel
gets a value; check those.

Output: <out>/energy_haNN.csv: comment lines, then ch,n,m,p0,p1,src,hits,pos_1[,pos_2 ...].
PSA_Calibration reads ch, n, m, p0, p1. Look at <out>/qa/energy_haNN.png before using it.
Needs numpy, matplotlib and PyROOT: /usr/bin/python3.
"""
import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from psa_calib_common import header, on_time_range, photopeak, read_tree


def parse_line(text):
    p = text.split(":")
    return (float(p[0]), None, None) if len(p) == 1 else (float(p[0]), float(p[1]), float(p[2]))


def line_position(counts, centers, line, peak_min):
    e, lo, hi = line
    if lo is None:
        return photopeak(counts, centers, peak_min)
    s = np.convolve(counts, np.ones(5) / 5, mode="same")
    w = (centers >= lo) & (centers <= hi)
    if not np.any(s[w] > 0):
        return np.nan
    return centers[w][np.argmax(s[w])]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("root", nargs="+", help="outputs of run_chain.py smile3_psa of source runs (with pixel, hg_corr)")
    ap.add_argument("-o", "--out-dir", required=True)
    ap.add_argument("--line", action="append", required=True, type=parse_line,
                    help="E [keV], or E:lo:hi (search between lo and hi of hg_corr); repeat for more lines")
    ap.add_argument("--boards", type=int, nargs="*")
    ap.add_argument("--min-hits", type=int, default=500, help="pixels with fewer hits use the sum of the ch")
    ap.add_argument("--hg-max", type=float, default=100e3)
    ap.add_argument("--hg-bins", type=int, default=250)
    ap.add_argument("--peak-min", type=float, default=15e3, help="photopeak above this (one line without lo:hi)")
    ap.add_argument("--peak-id-window", type=int, default=None)
    ap.add_argument("--no-qa", action="store_true")
    ap.add_argument("--overwrite", action="store_true")
    args = ap.parse_args()

    sel = "!satu_h && !satu_l && pixel >= 0"
    if args.boards:
        sel += " && (" + " || ".join(f"board == {b}" for b in args.boards) + ")"
    d = read_tree(args.root, ["board", "ch", "pix_n", "pix_m", "hg_corr", "peak_id"], sel)
    edges = np.linspace(0, args.hg_max, args.hg_bins + 1)
    centers = 0.5 * (edges[:-1] + edges[1:])
    energies = np.array([l[0] for l in args.line])
    os.makedirs(args.out_dir, exist_ok=True)

    for b in np.unique(d["board"]):
        out = os.path.join(args.out_dir, f"energy_ha{b:02d}.csv")
        if os.path.exists(out) and not args.overwrite:
            print(f"ha{b:02d}: {out} exists, skipped (--overwrite to replace)")
            continue
        m = d["board"] == b
        lo, hi = on_time_range(d["peak_id"][m], args.peak_id_window)
        m &= (d["peak_id"] >= lo) & (d["peak_id"] <= hi)
        rows, n_fallback = [], 0
        for c in range(6):
            mc = m & (d["ch"] == c)
            if not mc.any():
                continue
            ch_counts = np.histogram(d["hg_corr"][mc], bins=edges)[0]
            for n in range(1, 9):
                for mm in range(1, 9):
                    s = mc & (d["pix_n"] == n) & (d["pix_m"] == mm)
                    nh = int(s.sum())
                    counts, src = (np.histogram(d["hg_corr"][s], bins=edges)[0], "pixel") if nh >= args.min_hits \
                        else (ch_counts, "ch")
                    n_fallback += src == "ch"
                    pos = np.array([line_position(counts, centers, l, args.peak_min) for l in args.line])
                    if not np.all(np.isfinite(pos)) or np.any(pos <= 0):
                        continue
                    if len(pos) == 1:
                        p0, p1 = 0.0, energies[0] / pos[0]
                    else:
                        p1, p0 = np.polyfit(pos, energies, 1)
                    rows.append((c, n, mm, p0, p1, src, nh, pos))
        if not rows:
            print(f"ha{b:02d}: nothing written")
            continue
        lines = ", ".join(f"{e:g} keV" + (f" in {l:g}-{h:g}" if l is not None else f" (photopeak above {args.peak_min:g})")
                          for e, l, h in args.line)
        with open(out, "w") as f:
            f.write(header("make_psa_energy.py", args.root, [
                f"hits: {sel}, peak_id {lo}-{hi}; spectra of hg_corr ({args.hg_bins} bins up to {args.hg_max:g})",
                f"lines: {lines}", f"pixels with < {args.min_hits} hits use the sum of the ch: {n_fallback}"]))
            f.write("ch,n,m,p0,p1,src,hits," + ",".join(f"pos_{k + 1}" for k in range(len(args.line))) + "\n")
            for r in rows:
                f.write(f"{r[0]},{r[1]},{r[2]},{r[3]:.6g},{r[4]:.6g},{r[5]},{r[6]}," + ",".join(f"{p:.6g}" for p in r[7]) + "\n")
        print(f"ha{b:02d}: {out} ({len(rows)} pixels, {n_fallback} from the ch sum)")
        if not args.no_qa:
            import matplotlib
            matplotlib.use("Agg")
            import matplotlib.pyplot as plt
            os.makedirs(os.path.join(args.out_dir, "qa"), exist_ok=True)
            fig, axs = plt.subplots(1, 6, figsize=(30, 4))
            for c in range(6):
                g = np.full((8, 8), np.nan)
                for r in rows:
                    if r[0] == c:
                        g[r[1] - 1, r[2] - 1] = r[4]
                im = axs[c].imshow(g.T, origin="lower", extent=[0.5, 8.5, 0.5, 8.5])
                fig.colorbar(im, ax=axs[c], fraction=0.045)
                axs[c].set_title(f"ha{b:02d} ch{c}: p1 [keV / hg_corr]"); axs[c].set_xlabel("n"); axs[c].set_ylabel("m")
            fig.tight_layout()
            fig.savefig(os.path.join(args.out_dir, "qa", f"energy_ha{b:02d}.png"), dpi=60)
            plt.close(fig)


if __name__ == "__main__":
    sys.exit(main())
