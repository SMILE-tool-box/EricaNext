#!/usr/bin/env python3
"""Make the PSA energy calibration calib/psa/energy_haNN.csv from Cs-137 and Co-60 runs.

    /usr/bin/python3 make_psa_energy.py -o ../../calib/psa \\
        --cs psa_cal_20261103_run{01..11}.root --co psa_cal_20261103_run12.root

The inputs must be made with the position calibration (pixel) and the temperature gain table in place
(make_psa_position.py, make_psa_temp_gain.py): the spectra are of hg_corr = hg / gain(T), and PSA_Calibration
then gives energy = p0 + p1 hg_corr [keV] per pixel.

Same fits as analysis_sm3_FM/PSA_analysis/src_onoda/energy_calib.py (there per ch in a common 2 degC window of
hg; here per pixel, of hg_corr):
  - hits at the trigger time without saturation, with the temperature (temp_mean) inside the range of the
    temperature gain table of the ch (--temp-gain-dir; outside it the table only repeats its end value and
    hg_corr would be off by about 8 % per degC); a ch without a table (PSA_Calibration: hg_corr = hg) uses
    the hits of all temperatures,
  - Cs 661.66 keV: Gaussian + line in 0.8-1.25 x the highest-hg peak of the 5-bin smoothed spectrum above
    --peak-min with a prominence of at least --cs-prominence x the maximum (not the maximum itself: a bump at
    hg ~ 25000 can be higher than the Cs line; energy_calib.py: mean - 1 sigma .. mean + 2 sigma; too narrow for
    the statistics of a pixel),
  - Co 1173.23 / 1332.49 keV: the two lines are not resolved (one bump). Two Gaussians + line, the 1332 keV one
    at mean1 * r (r = 1.06-1.16) with width sigma1 sqrt(r) and height ratio 0.5-1.5; window 0.85-1.30 x the
    most prominent peak of the smoothed spectrum in 1.4-2.2 x the Cs position (not its maximum: the bump sits
    on the falling Compton part),
  - hg_corr = a E + b fitted to the 3 points (errors of the Gaussian centres, scaled by sqrt(chi2/ndf) if > 1),
    p1 = 1 / a, p0 = -b / a.
Each pixel (column src):
  pixel   Cs and Co fitted in the pixel; 1173 keV / 662 keV inside --co-ratio and a known to --p1-relerr,
  cs+ch   only the Cs line of the pixel (Co: fewer than --min-hits hits, fit failed or failed the checks):
          p0 of the ch sum, p1 = (661.66 - p0) / position of the pixel's Cs line,
  ch      no Cs line in the pixel: p0, p1 of the ch sum.
When the ch sum fails the Co checks (the Co spectrum of the ch is not the expected bump), the whole ch is
calibrated with the Cs line only (p0 = 0); the comment lines and the QA figure say so.
Without --co: p0 = 0 and p1 = 661.66 / Cs position.

Output: <out>/energy_haNN.csv: comment lines, then
ch,n,m,p0,p1,src,hits_cs,hits_co,pos_661.66,pos_1173.23,pos_1332.49,quality. PSA_Calibration reads ch, n, m,
p0, p1 (not quality), so pixels with a bad quality (the Cs line known worse than --p1-relerr) are not written
(energy = -1 for their hits); they are counted in the comment lines. Look at <out>/qa/energy_haNN.png.
Needs numpy, scipy, matplotlib and PyROOT: /usr/bin/python3.
"""
import argparse
import os
import sys

import numpy as np
from scipy.optimize import curve_fit
from scipy.signal import find_peaks

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from psa_calib_common import (N_BOARDS, board_peak_ranges, declare, header, hist_array,  # noqa: E402
                              peak_range_filter, rdataframe)

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
E_CS, E_CO1, E_CO2 = 661.66, 1173.23, 1332.49


def gauss(x, a, mu, s):
    return a * np.exp(-0.5 * ((x - mu) / s) ** 2)


def model_cs(x, a, mu, s, slope, off):
    return gauss(x, a, mu, s) + slope * (x - mu) + off


def model_co(x, a1, mu1, s1, a_ratio, mu_ratio, slope, off):
    """1173 keV at mu1, 1332 keV at mu1 * mu_ratio with width s1 sqrt(mu_ratio)"""
    return (gauss(x, a1, mu1, s1) + gauss(x, a1 * a_ratio, mu1 * mu_ratio, s1 * np.sqrt(mu_ratio))
            + slope * (x - mu1) + off)


def fit(model, centers, counts, window, p0, bounds):
    """fit inside window with Poisson errors -> (parameters, covariance) or None"""
    inside = (centers >= window[0]) & (centers <= window[1])
    if inside.sum() < len(p0) + 3 or counts[inside].sum() < 50:
        return None
    try:
        p, cov = curve_fit(model, centers[inside], counts[inside], p0=p0, bounds=bounds,
                           sigma=np.sqrt(np.maximum(counts[inside], 1)), absolute_sigma=True, maxfev=20000)
    except (RuntimeError, ValueError):
        return None
    if not np.all(np.isfinite(cov)):
        return None
    return p, cov


def smoothed_argmax(centers, counts, low, high):
    smooth = np.convolve(counts, np.ones(5) / 5, mode="same")
    inside = (centers >= low) & (centers <= high)
    if not inside.any() or smooth[inside].max() <= 0:
        return np.nan
    return centers[inside][np.argmax(smooth[inside])]


def last_prominent_peak(centers, counts, low, high, prominence):
    """position of the highest-hg peak of the 5-bin smoothed spectrum in [low, high] whose prominence is at least
    `prominence` x the maximum there; nan if none. The Cs line is the last peak of the spectrum, but it can be
    lower than the bump at hg ~ 25000 below it (seen in the 20261115 runs, in Cs and Co alike)."""
    smooth = np.convolve(counts, np.ones(5) / 5, mode="same")
    inside = (centers >= low) & (centers <= high)
    if not inside.any() or smooth[inside].max() <= 0:
        return np.nan
    s = smooth[inside]
    idx, _ = find_peaks(s, prominence=prominence * s.max())
    if len(idx) == 0:
        return centers[inside][np.argmax(s)]
    return centers[inside][idx.max()]


def most_prominent_peak(centers, counts, low, high):
    """position of the most prominent peak of the 5-bin smoothed spectrum in [low, high] (the Co bump sits on
    the falling Compton part: the maximum there is the low end of the range); the maximum if there is no peak"""
    smooth = np.convolve(counts, np.ones(5) / 5, mode="same")
    inside = (centers >= low) & (centers <= high)
    if not inside.any() or smooth[inside].max() <= 0:
        return np.nan
    s = smooth[inside]
    idx, prop = find_peaks(s, prominence=0)
    if len(idx) == 0:
        return centers[inside][np.argmax(s)]
    return centers[inside][idx[np.argmax(prop["prominences"])]]


class FitPos(tuple):
    """(mean, mean_err, sigma) of a line, with the fit it came from (model, params, window) for the QA figure"""
    def __new__(cls, values, model=None, params=None, window=None):
        t = super().__new__(cls, values)
        t.model, t.params, t.window = model, params, window
        return t


def fit_cs(centers, counts, peak_min, hg_max, prominence):
    """Cs line -> (mean, mean_err, sigma) or None. Window 0.8-1.25 x the last prominent peak of the smoothed
    spectrum (last_prominent_peak; energy_calib.py: mean - 1 sigma .. mean + 2 sigma, 3 times; with the few
    thousand hits of a pixel the Gaussian and the line background are degenerate in that narrow window and the
    fit runs away)."""
    peak = last_prominent_peak(centers, counts, peak_min, hg_max, prominence)
    if not np.isfinite(peak):
        return None
    window = (0.8 * peak, 1.25 * peak)
    height = counts[np.argmin(abs(centers - peak))]
    bounds = ([0, window[0], 0.01 * peak, -np.inf, -np.inf], [np.inf, window[1], 0.3 * peak, np.inf, np.inf])
    out = fit(model_cs, centers, counts, window, [max(height, 1), peak, 0.06 * peak, 0, 0], bounds)
    if out is None:
        return None
    p, cov = out
    return FitPos((p[1], np.sqrt(cov[1, 1]), abs(p[2])), model_cs, p, window)


def fit_co(centers, counts, cs_mean):
    """Co lines -> [(mean1, err1, sigma1), (mean2, err2, sigma2)] or None. The window stays at the bump (made
    wider from the fit, the 1173 keV Gaussian runs into the Compton part below)."""
    peak = most_prominent_peak(centers, counts, 1.4 * cs_mean, 2.2 * cs_mean)
    if not np.isfinite(peak):
        return None
    height = counts[np.argmin(abs(centers - peak))]
    p0 = [max(0.7 * height, 1), 0.96 * peak, 0.05 * peak, 0.9, 1.11, 0, 0]
    bounds = ([0, 0.80 * peak, 0.01 * peak, 0.5, 1.06, -np.inf, -np.inf],
              [np.inf, 1.10 * peak, 0.15 * peak, 1.5, 1.16, np.inf, np.inf])
    window = (0.85 * peak, 1.30 * peak)
    out = fit(model_co, centers, counts, window, p0, bounds)
    if out is None:
        return None
    p, cov = out
    m1, s1, r = p[1], p[2], p[4]
    j = np.zeros(len(p)); j[1], j[4] = r, m1          # d(mean2) / d(mean1, r)
    return [FitPos((m1, np.sqrt(cov[1, 1]), s1), model_co, p, window),
            FitPos((m1 * r, np.sqrt(j @ cov @ j), s1 * np.sqrt(r)))]


def line(pos):
    """hg_corr = a E + b to the 3 points -> p0, p1, relative error of p1"""
    e = np.array([E_CS, E_CO1, E_CO2])
    mu = np.array([p[0] for p in pos])
    w = 1 / np.maximum(np.array([p[1] for p in pos]), 1e-9) ** 2
    d = np.vstack([e, np.ones(3)]).T
    cov = np.linalg.inv(d.T @ (d * w[:, None]))
    a, b = cov @ d.T @ (w * mu)
    chi2 = float(np.sum(w * (mu - (a * e + b)) ** 2))
    a_err = np.sqrt(max(chi2, 1.0)) * np.sqrt(cov[0, 0])         # ndf = 1
    return -b / a, 1 / a, a_err / abs(a)


def temp_ranges(temp_gain_dir, board):
    """{ch: (tmin, tmax)} of the temperature gain table (bin centres +- 0.5 degC); {} without the file"""
    path = os.path.join(temp_gain_dir, f"temp_gain_ha{board:02d}.csv") if temp_gain_dir else ""
    if not path or not os.path.exists(path):
        return {}
    r = {}
    for row in open(path):
        if row.startswith("#") or row.startswith("ch"):
            continue
        ch, t = row.strip().split(",")[:2]
        lo, hi = r.get(int(ch), (np.inf, -np.inf))
        r[int(ch)] = (min(lo, float(t)), max(hi, float(t)))
    return {c: (lo - 0.5, hi + 0.5) for c, (lo, hi) in r.items()}


def spectra(files, sel, temp_gain_dir, hg_max, hg_bins, boards=None):
    """hg_corr spectra filled inside RDataFrame (the hits are never held in memory: a run of 10^8 hits as
    numpy arrays did not fit in 16 GB). Two passes over the files:
      1. peak_id per board -> the trigger-time range of the board (on_time_range),
      2. hits of `sel` at the trigger time of their board, with temp_mean inside the temperature gain table of
         their ch (all temperatures for a ch without a table), filled per (board, ch) and per (board, ch, pixel).
    Returns dict(boards, pid {board: (lo, hi)}, ch [board][ch] -> counts, px [board][ch][n-1][m-1] -> counts,
    hits [board][ch][n-1][m-1] -> number of hits incl. those outside 0..hg_max). Spectra are int64 arrays
    with np.histogram's bins (np.linspace(0, hg_max, hg_bins + 1))."""
    df = rdataframe(files, sel)
    pid = board_peak_ranges(df, boards)
    found = sorted(pid)
    tlo, thi = np.full(N_BOARDS * 6, -np.inf), np.full(N_BOARDS * 6, np.inf)
    for b in found:
        tr = temp_ranges(temp_gain_dir, b)
        for c in range(6):
            tlo[b * 6 + c], thi[b * 6 + c] = tr.get(c, (-np.inf, np.inf))
    cpp = lambda a: ", ".join("-INFINITY" if v == -np.inf else "INFINITY" if v == np.inf else repr(float(v)) for v in a)
    in_temp = declare(lambda name: f"""
    #include <cmath>
    bool {name}(int b, int c, double t) {{
        static const double lo[] = {{{cpp(tlo)}}}, hi[] = {{{cpp(thi)}}};
        return b >= 0 && b < {N_BOARDS} && c >= 0 && c < 6 && t >= lo[b * 6 + c] && t <= hi[b * 6 + c];
    }}""")
    nb = max(found) + 1 if found else 1
    d = (df.Filter(f"{peak_range_filter(pid)}(board, peak_id) && {in_temp}(board, ch, temp_mean)")
         .Define("key_ch", "board * 6 + ch")
         .Define("key_px", "(pix_n >= 1 && pix_n <= 8 && pix_m >= 1 && pix_m <= 8) ? "
                           "(board * 6 + ch) * 64 + (pix_n - 1) * 8 + (pix_m - 1) : -1"))
    hch = d.Histo2D(("ch", "", nb * 6, 0, nb * 6, hg_bins, 0, hg_max), "key_ch", "hg_corr")
    hpx = d.Histo2D(("px", "", nb * 384, 0, nb * 384, hg_bins, 0, hg_max), "key_px", "hg_corr")
    ach = hist_array(hch.GetValue(), nb * 6, hg_bins)[1:-1]
    apx = hist_array(hpx.GetValue(), nb * 384, hg_bins)[1:-1]
    # np.histogram puts hg_corr == hg_max in the last bin; TH2 puts it in the overflow (negligible for a double)
    return dict(boards=found, pid=pid,
                ch=ach[:, 1:-1].reshape(nb, 6, hg_bins),
                px=apx[:, 1:-1].reshape(nb, 6, 8, 8, hg_bins),
                hits=apx.sum(axis=1).reshape(nb, 6, 8, 8))


def calibrate(cs_spec, co_spec, centers, args, ch_line=None):
    """one pair of spectra -> dict(p0, p1, src, pos, quality); ch_line = (p0, p1) of the ch sum for the
    fallback (None: this is the ch sum)"""
    min_hits = args.min_hits if ch_line is not None else 0
    enough = lambda s: s is not None and s.sum() >= min_hits
    cs = fit_cs(centers, cs_spec, args.peak_min, args.hg_max, args.cs_prominence) if enough(cs_spec) else None
    if cs is None:
        return None if ch_line is None else dict(p0=ch_line[0], p1=ch_line[1], src="ch", pos=[], quality="ok")
    co = fit_co(centers, co_spec, cs[0]) if enough(co_spec) else None
    if co is not None:
        bad = []
        if not (args.co_ratio[0] <= co[0][0] / cs[0] <= args.co_ratio[1]):
            bad.append("co_ratio")
        p0, p1, relerr = line([cs] + co)
        if relerr > args.p1_relerr:
            bad.append("p1_err")
        if not bad or ch_line is None:
            return dict(p0=p0, p1=p1, src="pixel" if ch_line is not None else "cs+co", pos=[cs] + co,
                        quality="ok" if not bad else "bad:" + "+".join(bad))
    p0 = 0.0 if ch_line is None else ch_line[0]
    p1 = (E_CS - p0) / cs[0]
    q = "ok" if cs[1] / cs[0] <= args.p1_relerr else "bad:p1_err"
    return dict(p0=p0, p1=p1, src="cs" if ch_line is None else "cs+ch", pos=[cs], quality=q)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out-dir", required=True)
    ap.add_argument("--cs", nargs="+", required=True, help="smile3_psa outputs of the Cs-137 runs")
    ap.add_argument("--co", nargs="+", help="smile3_psa outputs of the Co-60 runs (without: p0 = 0)")
    ap.add_argument("--temp-gain-dir", default=os.path.join(REPO, "calib", "psa"),
                    help="temperature gain tables used for the inputs (only hits inside their temperature range); "
                         "'' if the inputs were made without them (all hits)")
    ap.add_argument("--boards", type=int, nargs="*")
    ap.add_argument("--min-hits", type=int, default=1000, help="fewer hits in a pixel (per source): not fitted")
    ap.add_argument("--hg-max", type=float, default=350e3,
                    help="upper end of the spectra (the Co lines are at hg_corr 1.5e5-1.7e5 on high-gain boards)")
    ap.add_argument("--hg-bins", type=int, default=1400)
    ap.add_argument("--peak-min", type=float, default=15e3, help="the Cs line is looked for above this")
    ap.add_argument("--cs-prominence", type=float, default=0.1,
                    help="the Cs line is the highest-hg peak with a prominence of at least this x the maximum")
    ap.add_argument("--co-ratio", type=float, nargs=2, default=(1.55, 1.95),
                    help="allowed (1173 keV position) / (662 keV position) [energy_calib.py]")
    ap.add_argument("--p1-relerr", type=float, default=0.03, help="allowed relative error of p1 [energy_calib.py]")
    ap.add_argument("--no-qa", action="store_true")
    ap.add_argument("--overwrite", action="store_true")
    args = ap.parse_args()

    sel = "!satu_h && !satu_l && pixel >= 0"
    if args.boards:
        sel += " && (" + " || ".join(f"board == {b}" for b in args.boards) + ")"
    edges = np.linspace(0, args.hg_max, args.hg_bins + 1)
    centers = 0.5 * (edges[:-1] + edges[1:])
    data = [spectra(f, sel, args.temp_gain_dir, args.hg_max, args.hg_bins, args.boards)
            for f in [args.cs] + ([args.co] if args.co else [])]
    os.makedirs(args.out_dir, exist_ok=True)

    def spec(k, b, c, n=None, m=None):
        """spectrum of source k (0 Cs, 1 Co) of the ch sum (n None) or of the pixel; None: no such source"""
        if k >= len(data):
            return None
        d = data[k]
        if b not in d["pid"]:
            return np.zeros(args.hg_bins, dtype=np.int64)
        return d["ch"][b, c] if n is None else d["px"][b, c, n - 1, m - 1]

    def n_hits(k, b, c, n, m):
        d = data[k]
        return int(d["hits"][b, c, n - 1, m - 1]) if b in d["pid"] else 0

    for b in data[0]["boards"]:
        out = os.path.join(args.out_dir, f"energy_ha{b:02d}.csv")
        if os.path.exists(out) and not args.overwrite:
            print(f"ha{b:02d}: {out} exists, skipped (--overwrite to replace)")
            continue
        trange = temp_ranges(args.temp_gain_dir, b)
        pid = [d["pid"].get(b, (0, -1)) for d in data]
        rows, qa = [], {}
        no_table = [c for c in range(6) if c not in trange]
        if no_table:
            print(f"ha{b:02d}: no temperature gain table for ch {no_table} "
                  f"(PSA_Calibration: hg_corr = hg); all temperatures used")
        for c in range(6):
            # without a table PSA_Calibration does not correct (hg_corr = hg): no temperature cut (in spectra())
            ch_cs, ch_co = spec(0, b, c), spec(1, b, c)
            ch = calibrate(ch_cs, ch_co, centers, args)
            if ch is None:
                print(f"ha{b:02d} ch{c}: no Cs line in the ch sum; ch skipped")
                continue
            use_co = bool(args.co)
            if use_co and ch["quality"] != "ok":
                # the Co spectrum of the ch is not the expected bump (e.g. gain outside the table): Cs only
                bad_co = ch
                ch = calibrate(ch_cs, None, centers, args)
                ch.update(quality=f"Co {bad_co['quality']}, Cs only", co_pos=bad_co["pos"][1:])
                use_co = False
            if args.co and not use_co:
                print(f"ha{b:02d} ch{c}: Co not usable in the ch sum ({ch['quality']}); p0 = 0 for this ch")
            qa[c] = dict(ch, spec=(ch_cs, ch_co))
            for n in range(1, 9):
                for mm in range(1, 9):
                    r = calibrate(spec(0, b, c, n, mm), spec(1, b, c, n, mm) if use_co else None, centers, args,
                                  ch_line=(ch["p0"], ch["p1"]))
                    hits = [n_hits(k, b, c, n, mm) for k in range(len(data))] + ([] if args.co else [0])
                    rows.append((c, n, mm, r, hits))
        if not rows:
            print(f"ha{b:02d}: nothing written")
            continue
        count = {s: sum(r[3]["src"] == s for r in rows) for s in ("pixel", "cs+ch", "ch")}
        n_bad = sum(r[3]["quality"] != "ok" for r in rows)
        summary = ", ".join(f"{k} {v}" for k, v in count.items()) + f"; quality bad: {n_bad}"
        info = [f"hits: {sel}, at the trigger time (peak_id {pid[0][0]}-{pid[0][1]} Cs" +
                (f", {pid[1][0]}-{pid[1][1]} Co)" if args.co else ")") +
                f", temp_mean inside the temperature gain table of the ch ({args.temp_gain_dir})" +
                (f"; ch {no_table} without a table: all temperatures (hg_corr = hg)" if no_table else ""),
                f"spectra of hg_corr ({args.hg_bins} bins up to {args.hg_max:g}); Cs 661.66 keV: " +
                ", ".join(os.path.basename(x) for x in args.cs) +
                ("; Co 1173.23, 1332.49 keV: " + ", ".join(os.path.basename(x) for x in args.co) if args.co else ""),
                "E = p0 + p1 hg_corr from the 3 lines (src pixel), from the pixel's Cs line and p0 of the ch sum "
                "(cs+ch), or of the ch sum (ch)" if args.co else "Cs line only: p0 = 0",
                "ch sums: " + "; ".join(f"ch{c} p0 {q['p0']:.1f} keV p1 {q['p1']:.5g} ({q['src']}, {q['quality']})"
                                        for c, q in qa.items()),
                "pixels: " + summary + " (bad ones not written)"]
        with open(out, "w") as f:
            f.write(header("make_psa_energy.py", args.cs + (args.co or []), info))
            f.write(f"ch,n,m,p0,p1,src,hits_cs,hits_co,pos_{E_CS:g},pos_{E_CO1:g},pos_{E_CO2:g},quality\n")
            for c, n, mm, r, hits in rows:
                if r["quality"] != "ok":
                    continue
                pos = [p[0] for p in r["pos"]] + [np.nan] * (3 - len(r["pos"]))
                f.write(f"{c},{n},{mm},{r['p0']:.6g},{r['p1']:.6g},{r['src']},{hits[0]},{hits[1]}," +
                        ",".join(f"{p:.6g}" for p in pos) + f",{r['quality']}\n")
        print(f"ha{b:02d}: {out} ({len(rows)} pixels: {summary})")
        if not args.no_qa:
            qa_figure(os.path.join(args.out_dir, "qa", f"energy_ha{b:02d}.png"), b, rows, qa, centers)


def qa_figure(path, b, rows, qa, centers):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fig, axs = plt.subplots(4, 6, figsize=(30, 17))
    for c in range(6):
        if c not in qa:
            for a in axs[:, c]:
                a.axis("off")
            continue
        q = qa[c]
        ax = axs[0, c]
        for s, lab in zip(q["spec"], ("Cs-137", "Co-60")):
            if s is not None:
                ax.step(centers, s, where="mid", lw=0.8, label=lab)
        # fitted curves: the whole model (solid) and its Gaussians and background (dotted)
        for f in ([q["pos"][0]] + (q["pos"][1:2] or q.get("co_pos", [])[:1])):
            if getattr(f, "params", None) is None:
                continue
            xx = np.linspace(*f.window, 300)
            ax.plot(xx, f.model(xx, *f.params), color="k", lw=1)
            if f.model is model_cs:
                a, mu, sg, slope, off = f.params
                parts = [gauss(xx, a, mu, sg)]
                bg = slope * (xx - mu) + off
            else:
                a1, mu1, s1, ar, mr, slope, off = f.params
                parts = [gauss(xx, a1, mu1, s1), gauss(xx, a1 * ar, mu1 * mr, s1 * np.sqrt(mr))]
                bg = slope * (xx - mu1) + off
            for g in parts:
                ax.plot(xx, g, ":", color="C2", lw=1.2)
            ax.plot(xx, bg, ":", color="gray", lw=1)
        for (mu, err, sg), e in zip(q["pos"] + q.get("co_pos", []), (E_CS, E_CO1, E_CO2)):
            ax.axvline(mu, color="r", lw=0.6, ls="-" if e == E_CS or "co_pos" not in q else ":")
            ax.text(mu, 0.97, f"{e:g}", color="r", fontsize=7, rotation=90, va="top",
                    transform=ax.get_xaxis_transform())
        ax.set_xlim(0, min(centers[-1], 2.6 * q["pos"][0][0])); ax.set_xlabel("hg_corr"); ax.legend(fontsize=7)
        ax.set_title(f"ha{b:02d} ch{c}: ch sum ({q['src']}, {q['quality']})")
        ax = axs[1, c]
        mu = np.array([p[0] for p in q["pos"]])
        es = np.array([E_CS, E_CO1, E_CO2][:len(mu)])
        x = np.linspace(0, mu.max() * 1.15, 10)
        ax.plot(x, q["p0"] + q["p1"] * x, "k-", lw=0.8)
        ax.plot(mu, es, "ro")
        ax.text(0.03, 0.97, f"p0 = {q['p0']:.1f} keV\np1 = {q['p1'] * 1e3:.3f} keV / 1000\nresidual [keV]: " +
                ", ".join(f"{q['p0'] + q['p1'] * m - e:+.1f}" for m, e in zip(mu, es)),
                transform=ax.transAxes, va="top", fontsize=8)
        ax.set_xlabel("hg_corr"); ax.set_ylabel("E [keV]"); ax.set_title("ch sum: E = p0 + p1 hg_corr")
        for k, (key, title) in enumerate((("p1", "p1 [keV / 1000 hg_corr]"), ("p0", "p0 [keV]"))):
            g = np.full((8, 8), np.nan)
            src = {}
            for cc, n, m, r, _ in rows:
                if cc == c and r["quality"] == "ok":
                    g[n - 1, m - 1] = r[key] * (1e3 if key == "p1" else 1)
                    src[(n, m)] = r["src"]
            a = axs[2 + k, c]
            im = a.imshow(g.T, origin="lower", extent=[0.5, 8.5, 0.5, 8.5])
            fig.colorbar(im, ax=a, fraction=0.045)
            for (n, m), s in src.items():
                if s != "pixel":
                    a.text(n, m, {"cs+ch": "c", "ch": "x"}[s], ha="center", va="center", fontsize=8, color="w")
            a.set_title(title + " (white: bad, c: cs+ch, x: ch)", fontsize=9); a.set_xlabel("n"); a.set_ylabel("m")
    fig.tight_layout()
    fig.savefig(path, dpi=60)
    plt.close(fig)


if __name__ == "__main__":
    sys.exit(main())
