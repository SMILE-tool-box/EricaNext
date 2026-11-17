#!/usr/bin/env python3
"""Pick TPC tracks (the longest by default) from a raw ROOT (RawTreeWriter, TPC_RawTreeWriter/raw) and draw them,
one PNG per event.

    /usr/bin/python3 plot_tpc_long_tracks.py <raw.root> -o <out dir> [--scan 300000] [--top 8]
        [--drift-file drift_velocity.dat] [--min-length 5 --max-length 30 --order random]

- Removed first: hot strips (hit in more than --hot-frac of the first --hot-events events), and in each event the
  strips hit in more than --max-strip-clocks clocks (a strip stuck on for that event).
- An event is used when its anode and cathode hits are in one micro-PIC (384 strips each side), with at least
  --min-hits hits on each side.
- The track is the set of 3D points of anode x cathode hits of the same clock (noise on one side only does not
  make it long). Its clocks must be continuous: distinct clocks of the points / their clock range >= --min-fill.
- Length = sqrt(dx^2 + dy^2 + dz^2) of the 1-99 % ranges of the 3D points.
- Coordinates are local to the micro-PIC: anode direction (strip % 384 x pitch), cathode direction, and the drift
  direction z = v (clock - clock_offset) + z_gem. The micro-PIC layout of the detector frame is not used (not yet
  confirmed for SMILE-3). v and clock_offset: --drift-file (make_tpc_drift_velocity.py) or --velocity/--clock-offset.
- Events with a length in [--min-length, --max-length] are kept; --order long (longest first), short (shortest
  first) or random (a random sample, --seed) decides which --top of them are drawn.
- Writes track_<rank>_evt<event_id>.png and tracks.csv (all kept events, in the drawing order).
"""
import argparse
import csv
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import ROOT

from tpc_raw_io import raw_tree_path

STRIPS_PER_UPIC = 384


def read_drift(path):
    vals = {}
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        k, v = line.split()[:2]
        vals[k] = float(v)
    return vals["velocity"], vals["clock_offset"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("raw")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--first", type=int, default=0, help="first row to scan")
    ap.add_argument("--scan", type=int, default=300000, help="rows to scan (-1: all)")
    ap.add_argument("--top", type=int, default=8, help="number of events to draw")
    ap.add_argument("--hot-events", type=int, default=20000, help="rows used to find hot strips")
    ap.add_argument("--hot-frac", type=float, default=0.2, help="a strip hit in more than this fraction is hot")
    ap.add_argument("--max-strip-clocks", type=int, default=300,
                    help="in an event, a strip hit in more clocks than this is removed")
    ap.add_argument("--min-hits", type=int, default=20, help="minimum (strip, clock) hits on each side")
    ap.add_argument("--min-fill", type=float, default=0.6, help="minimum distinct clocks / clock range")
    ap.add_argument("--pitch", type=float, default=0.4, help="strip pitch [mm]")
    ap.add_argument("--drift-file", help="drift_velocity.dat (velocity [mm/clock], clock_offset [clock])")
    ap.add_argument("--velocity", type=float, help="drift velocity [mm/clock] (without --drift-file)")
    ap.add_argument("--clock-offset", type=float, help="clock of the GEM [clock] (without --drift-file)")
    ap.add_argument("--z-gem", type=float, default=10.0, help="z of the GEM [mm]")
    ap.add_argument("--min-length", type=float, default=0.0, help="minimum track length [mm]")
    ap.add_argument("--max-length", type=float, default=float("inf"), help="maximum track length [mm]")
    ap.add_argument("--order", choices=("long", "short", "random"), default="long", help="which events to draw")
    ap.add_argument("--seed", type=int, default=1, help="random seed for --order random")
    a = ap.parse_args()
    if a.drift_file:
        v, off = read_drift(a.drift_file)
        zsrc = f"{Path(a.drift_file).name}: v = {v:.4f} mm/clock, offset = {off:.1f} clock"
    elif a.velocity is not None and a.clock_offset is not None:
        v, off = a.velocity, a.clock_offset
        zsrc = f"v = {v:.4f} mm/clock, offset = {off:.1f} clock"
    else:
        ap.error("give --drift-file, or --velocity and --clock-offset")
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)

    f = ROOT.TFile.Open(a.raw)
    t = f.Get(raw_tree_path(a.raw))
    t.SetBranchStatus("*", 0)
    for b in ("event_id", "hit_a_strip", "hit_a_clock", "hit_c_strip", "hit_c_clock"):
        t.SetBranchStatus(b, 1)
    n = t.GetEntries()
    last = n if a.scan < 0 else min(n, a.first + a.scan)

    def points(sa, ca, sc, cc):
        """3D points (anode strip, cathode strip, clock) of the same clock"""
        pa = {}
        for s_, c_ in zip(sa, ca):
            pa.setdefault(c_, []).append(s_)
        out = [(x, y, c_) for y, c_ in zip(sc, cc) for x in pa.get(c_, ())]
        return np.array(out, dtype=np.int32).reshape(-1, 3)

    def clean(i):
        """hits of row i without hot strips and strips stuck on in the event"""
        sa, ca, sc, cc = hits(i)
        ka, kc = ~hot_a[sa], ~hot_c[sc]
        sa, ca, sc, cc = sa[ka], ca[ka], sc[kc], cc[kc]
        na = np.bincount(sa, minlength=4 * STRIPS_PER_UPIC)
        nc = np.bincount(sc, minlength=4 * STRIPS_PER_UPIC)
        ka, kc = na[sa] <= a.max_strip_clocks, nc[sc] <= a.max_strip_clocks
        return sa[ka], ca[ka], sc[kc], cc[kc]

    def hits(i):
        t.GetEntry(i)
        return (np.asarray(t.hit_a_strip, dtype=np.int32), np.asarray(t.hit_a_clock, dtype=np.int32),
                np.asarray(t.hit_c_strip, dtype=np.int32), np.asarray(t.hit_c_clock, dtype=np.int32))

    # hot strips
    occ_a = np.zeros(4 * STRIPS_PER_UPIC)
    occ_c = np.zeros(4 * STRIPS_PER_UPIC)
    n_hot_ev = min(a.hot_events, last - a.first)
    for i in range(a.first, a.first + n_hot_ev):
        sa, _, sc, _ = hits(i)
        occ_a[np.unique(sa)] += 1
        occ_c[np.unique(sc)] += 1
    hot_a = occ_a > a.hot_frac * n_hot_ev
    hot_c = occ_c > a.hot_frac * n_hot_ev
    print(f"hot strips: anode {np.flatnonzero(hot_a).tolist()}, cathode {np.flatnonzero(hot_c).tolist()}")

    rows = []
    for i in range(a.first, last):
        sa, ca, sc, cc = clean(i)
        if len(sa) < a.min_hits or len(sc) < a.min_hits:
            continue
        ua, uc = sa // STRIPS_PER_UPIC, sc // STRIPS_PER_UPIC
        p = np.bincount(ua, minlength=4).argmax()
        if np.mean(ua == p) < 0.95 or np.mean(uc == p) < 0.95:
            continue
        sa, ca, sc, cc = sa[ua == p], ca[ua == p], sc[uc == p], cc[uc == p]
        pts = points(sa, ca, sc, cc)
        if len(pts) < a.min_hits:
            continue
        lo, hi = np.percentile(pts, [1, 99], axis=0)
        if hi[2] - lo[2] < 1 or len(np.unique(pts[:, 2])) / (np.ptp(pts[:, 2]) + 1) < a.min_fill:
            continue
        d = (hi - lo) * np.array([a.pitch, a.pitch, v])
        length = float(np.sqrt((d ** 2).sum()))
        if not a.min_length <= length <= a.max_length:
            continue
        rows.append({"row": i, "event_id": int(t.event_id), "upic": int(p), "length_mm": round(length, 1),
                     "n_anode_hits": len(sa), "n_cathode_hits": len(sc), "n_points": len(pts)})
    if a.order == "random":
        np.random.default_rng(a.seed).shuffle(rows)
    else:
        rows.sort(key=lambda r: -r["length_mm"] if a.order == "long" else r["length_mm"])
    with open(out / "tracks.csv", "w", newline="") as fo:
        fo.write(f"# {a.raw}, rows {a.first}-{last - 1}, {zsrc}, pitch {a.pitch} mm, "
                 f"length {a.min_length}-{a.max_length} mm, order {a.order}\n")
        w = csv.DictWriter(fo, fieldnames=list(rows[0]) if rows else ["row"])
        w.writeheader()
        w.writerows(rows)
    print(f"{len(rows)} events pass of {last - a.first}; drawing {min(a.top, len(rows))}")

    for rank, r in enumerate(rows[:a.top], 1):
        sa, ca, sc, cc = clean(r["row"])
        ka, kc = sa // STRIPS_PER_UPIC == r["upic"], sc // STRIPS_PER_UPIC == r["upic"]
        sa, ca, sc, cc = sa[ka], ca[ka], sc[kc], cc[kc]
        xa, za = a.pitch * (sa % STRIPS_PER_UPIC + 0.5), v * (ca - off) + a.z_gem
        yc, zc = a.pitch * (sc % STRIPS_PER_UPIC + 0.5), v * (cc - off) + a.z_gem
        p3 = points(sa, ca, sc, cc).astype(float)
        pts = np.column_stack([a.pitch * (p3[:, 0] % STRIPS_PER_UPIC + 0.5), a.pitch * (p3[:, 1] % STRIPS_PER_UPIC + 0.5),
                               v * (p3[:, 2] - off) + a.z_gem])

        fig = plt.figure(figsize=(15, 5))
        ax1 = fig.add_subplot(1, 3, 1)
        ax2 = fig.add_subplot(1, 3, 2)
        ax3 = fig.add_subplot(1, 3, 3, projection="3d")
        ax1.scatter(xa, za, s=4, c="#1f5fa8", marker="s", lw=0)
        ax2.scatter(yc, zc, s=4, c="#c0392b", marker="s", lw=0)
        ax1.set(xlabel="anode direction [mm]", ylabel="drift direction z [mm]", title="anode")
        ax2.set(xlabel="cathode direction [mm]", title="cathode")
        if len(pts):
            ax3.scatter(pts[:, 0], pts[:, 1], pts[:, 2], s=3, c=pts[:, 2], cmap="viridis", lw=0)
        ax3.set(xlabel="anode [mm]", ylabel="cathode [mm]", zlabel="z [mm]", title="3D (same-clock pairs)")
        zlo, zhi = min(za.min(), zc.min()), max(za.max(), zc.max())
        for ax, u in ((ax1, xa), (ax2, yc)):
            # same mm scale on both axes, centred on the hits
            span = max(np.ptp(u), zhi - zlo)
            half = span / 2 + max(3.0, 0.1 * span)
            ax.set_xlim(u.mean() - half, u.mean() + half)
            ax.set_ylim((zlo + zhi) / 2 - half, (zlo + zhi) / 2 + half)
            ax.set_aspect("equal")
            ax.grid(alpha=0.3)
        fig.suptitle(f"event {r['event_id']} (row {r['row']}), micro-PIC {r['upic']}, length {r['length_mm']:.1f} mm",
                     fontsize=12)
        fig.text(0.5, 0.005, f"{Path(a.raw).name}; {zsrc}; pitch {a.pitch} mm; hot strips removed",
                 ha="center", fontsize=8, color="gray")
        fig.tight_layout(rect=(0, 0.05, 1, 0.95))
        name = out / f"track_{rank:02d}_evt{r['event_id']}.png"
        fig.savefig(name, dpi=200)
        plt.close(fig)
        print(name)


if __name__ == "__main__":
    main()
