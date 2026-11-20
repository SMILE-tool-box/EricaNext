#!/usr/bin/env python3
"""Event display of ETCC events: the TPC track points, the TOT_Skewness result and the HA hit, one PNG per event
(whole detector: 3D, X-Y, X-Z, Y-Z) and one zoomed on the track (<out>_evt<id>.png, <out>_evt<id>_zoom.png).

    python3 run_script/run_chain.py smile3_reconstruct --root-input <etcc raw.root> -o recon_cloud.root \
        --first-entry 2000000 -n 20000 --set TOT_Skewness.flag_save_cloud=true
    /usr/bin/python3 scripts/analysis/plot_etcc_event.py recon_cloud.root calib/psa/geometry.csv <out prefix> [evt_id ...]
        [--min-len 15 --max-len 120 --top 6 --order points|length]

- Input: a smile3_reconstruct output made with TOT_Skewness.flag_save_cloud=true (TOT_Skewness/tot_skewness with
  cloud_x/y/z) and Reconstruct/single_hit. The rows of the two trees are matched by Pe (Reconstruct copies it;
  Ve is written there as a unit vector, so it is only checked to be parallel).
- Without evt_id: up to --top single_hit events with a track, >= 40 points, Len in [--min-len, --max-len] mm,
  Ke 30-400 keV and a fitted Ve_z (not the in-plane fallback of TOT_Skewness, Ve_z = 0), the ones with the most
  points (--order points) or the longest (--order length) first.
- Drawn: track points (blue), Pe (orange star) and the line through it along Ve (orange), Pg (green square),
  Pe -> Pg (green dashed), the TPC drift volume (uPIC +-153.6 mm, Z 5-317 mm) and the HA blocks of geometry.csv.
- The track is in the frame of TOT_Skewness (centre of the uPIC, Z along the drift) and the HA in the frame of
  geometry.csv (X: E, Y: S, Z: down). That the two frames are the same is not yet checked (AGENTS.md).
- Needs PyROOT, numpy, matplotlib: /usr/bin/python3.
"""
import argparse, csv, numpy as np, ROOT, matplotlib
matplotlib.use("Agg"); import matplotlib.pyplot as plt
ROOT.gROOT.SetBatch(True)
ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("root"); ap.add_argument("geometry"); ap.add_argument("out"); ap.add_argument("evt", nargs="*", type=int)
ap.add_argument("--min-len", type=float, default=15.0, help="[mm]")
ap.add_argument("--max-len", type=float, default=120.0, help="[mm]")
ap.add_argument("--top", type=int, default=6)
ap.add_argument("--order", choices=("points", "length"), default="points")
args = ap.parse_args()
fn, geo, out, want = args.root, args.geometry, args.out, args.evt
f = ROOT.TFile.Open(fn)
sh = f.Get("Reconstruct/single_hit"); ts = f.Get("TOT_Skewness/tot_skewness")
# tot_skewness rows by Pe (Reconstruct copies Pe as it is, but writes Ve as a unit vector); Ve checked to be parallel
tot = {}
for t in ts:
    if t.Pe[0] > 1e8: continue
    key = tuple(round(v, 9) for v in t.Pe)
    tot.setdefault(key, []).append((np.array(t.cloud_x), np.array(t.cloud_y), np.array(t.cloud_z), t.Len, np.array(t.Ve)))
rows = []
for s in sh:
    if not s.has_track: continue
    key = tuple(round(v, 9) for v in s.Pe); ve = np.array(s.Ve)
    cand = [c for c in tot.get(key, []) if np.allclose(c[4] / np.linalg.norm(c[4]), ve, atol=1e-9)]
    if len(cand) == 1:
        rows.append(dict(evt=s.evt_id, Eg=s.Eg, Ke=s.Ke, Pe=np.array(s.Pe), Ve=ve, Pg=np.array(s.Pg),
                         cag=s.cos_alpha_geo, cak=s.cos_alpha_kin, cloud=cand[0][:3], Len=cand[0][3]))
print(f"single_hit with track and cloud: {len(rows)}")
if want:
    sel = [r for r in rows if r["evt"] in want]
else:   # clear tracks: many points, a few cm long, a TPC energy that makes a track
    sel = [r for r in rows if len(r["cloud"][0]) >= 40 and args.min_len <= r["Len"] <= args.max_len
           and 30 <= r["Ke"] <= 400 and r["Ve"][2] != 0]
    sel = sorted(sel, key=lambda r: -(len(r["cloud"][0]) if args.order == "points" else r["Len"]))[:args.top]
for r in sel:
    print(f"evt {r['evt']}: Eg {r['Eg']:.0f} keV, Ke {r['Ke']:.1f} keV, points {len(r['cloud'][0])}, Len {r['Len']:.1f} mm, "
          f"Pe {np.round(r['Pe'], 1)}, Ve {np.round(r['Ve'], 3)}, Pg {np.round(r['Pg'], 1)}, "
          f"cos_alpha geo {r['cag']:.3f} kin {r['cak']:.3f}")
blocks = [{k: float(v) for k, v in row.items()} for row in csv.DictReader(l for l in open(geo) if not l.startswith("#"))]
def block_corners(b):
    c = np.array([b["x0"], b["y0"], b["z0"]]); u = np.array([b["ux"], b["uy"], b["uz"]]); v = np.array([b["vx"], b["vy"], b["vz"]])
    h = 4 * b["pitch"]
    return np.array([c + su * h * u + sv * h * v for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1), (-1, -1))])
HX, ZG, ZT = 153.6, 5.0, 317.0      # uPIC half width (4 x 384 strips x 0.4 mm), GEM (TOT_Skewness.z_gem_mm) and top of the drift [mm]
box = [np.array(p) for p in ([-HX, -HX], [HX, -HX], [HX, HX], [-HX, HX], [-HX, -HX])]
proj = [("x", "y", 0, 1), ("x", "z", 0, 2), ("y", "z", 1, 2)]
for r in sel:
    x, y, z = r["cloud"]; Pe, Ve, Pg = r["Pe"], r["Ve"], r["Pg"]
    L = max(r["Len"], 20.0)
    fig = plt.figure(figsize=(20, 6.5))
    ax3 = fig.add_subplot(1, 4, 1, projection="3d")
    axes = [fig.add_subplot(1, 4, k + 2) for k in range(3)]
    # context: TPC volume and HA blocks (faint)
    for zz in (ZG, ZT):
        bx = np.array(box); ax3.plot(bx[:, 0], bx[:, 1], zz, color="#bbbbbb", lw=0.6)
    for b in blocks:
        cc = block_corners(b); ax3.plot(cc[:, 0], cc[:, 1], cc[:, 2], color="#cccccc", lw=0.4)
    ax3.scatter(x, y, z, s=4, c="#2a6fdb", alpha=0.7)
    ax3.plot(*[[Pe[i] - 0.5 * L * Ve[i], Pe[i] + L * Ve[i]] for i in range(3)], color="#e8590c", lw=2)
    ax3.scatter(*Pe, s=60, c="#e8590c", marker="*"); ax3.scatter(*Pg, s=60, c="#2b8a3e", marker="s")
    ax3.plot(*[[Pe[i], Pg[i]] for i in range(3)], color="#2b8a3e", lw=1, ls="--")
    ax3.set(xlabel="X [mm]", ylabel="Y [mm]", zlabel="Z [mm]"); ax3.invert_zaxis()
    for ax, (a, bname, i, j) in zip(axes, proj):
        if (i, j) == (0, 1):
            bx = np.array(box); ax.plot(bx[:, 0], bx[:, 1], color="#bbbbbb", lw=0.8)
        else:
            ax.plot([-HX, HX, HX, -HX, -HX], [ZG, ZG, ZT, ZT, ZG], color="#bbbbbb", lw=0.8)
        for b in blocks:
            cc = block_corners(b); ax.plot(cc[:, i], cc[:, j], color="#dddddd", lw=0.5)
        ax.plot([Pe[i], Pg[i]], [Pe[j], Pg[j]], color="#2b8a3e", lw=1, ls="--", label="Pe → Pg (scattered gamma)")
        ax.scatter(Pg[i], Pg[j], s=50, c="#2b8a3e", marker="s", label="Pg (HA hit)", zorder=4)
        # inset-like zoom is the 4th panel set below; here the whole detector
        ax.scatter([x, y, z][i], [x, y, z][j], s=3, c="#2a6fdb", label="track points", zorder=3)
        ax.plot([Pe[i] - 0.5 * L * Ve[i], Pe[i] + L * Ve[i]], [Pe[j] - 0.5 * L * Ve[j], Pe[j] + L * Ve[j]],
                color="#e8590c", lw=2, label="TOT_Skewness: Pe, Ve")
        ax.scatter(Pe[i], Pe[j], s=80, c="#e8590c", marker="*", zorder=5)
        ax.set(xlabel=f"{a.upper()} [mm]", ylabel=f"{bname.upper()} [mm]", aspect="equal")
        if j == 2: ax.invert_yaxis()
    axes[0].legend(fontsize=7, loc="upper left")
    fig.suptitle(f"evt {r['evt']}: Eg {r['Eg']:.0f} keV, Ke {r['Ke']:.1f} keV, {len(x)} points, Len {r['Len']:.1f} mm, "
                 f"cos α geo {r['cag']:.2f} / kin {r['cak']:.2f}   (TPC frame of TOT_Skewness; HA from geometry.csv, X: E, Y: S, Z: down)")
    fig.tight_layout(); fig.savefig(f"{out}_evt{r['evt']}.png", dpi=80); plt.close(fig)
    # zoom on the track
    fig, axs = plt.subplots(1, 3, figsize=(16, 5.5))
    for ax, (a, bname, i, j) in zip(axs, proj):
        c = [x, y, z]
        ax.scatter(c[i], c[j], s=8, c="#2a6fdb", label="track points")
        ax.plot([Pe[i] - 0.5 * L * Ve[i], Pe[i] + L * Ve[i]], [Pe[j] - 0.5 * L * Ve[j], Pe[j] + L * Ve[j]], color="#e8590c", lw=2, label="Ve through Pe")
        ax.scatter(Pe[i], Pe[j], s=120, c="#e8590c", marker="*", label="Pe", zorder=5)
        d = Pg - Pe; d = d / np.linalg.norm(d)
        ax.plot([Pe[i], Pe[i] + L * d[i]], [Pe[j], Pe[j] + L * d[j]], color="#2b8a3e", ls="--", lw=1, label="towards Pg")
        lo_i, hi_i = min(c[i].min(), Pe[i]), max(c[i].max(), Pe[i]); lo_j, hi_j = min(c[j].min(), Pe[j]), max(c[j].max(), Pe[j])
        m = max(hi_i - lo_i, hi_j - lo_j, 20) * 0.6; mi, mj = 0.5 * (lo_i + hi_i), 0.5 * (lo_j + hi_j)
        ax.set(xlim=(mi - m, mi + m), ylim=(mj - m, mj + m),
               xlabel=f"{a.upper()} [mm]", ylabel=f"{bname.upper()} [mm]", aspect="equal")
        if j == 2: ax.invert_yaxis()
    axs[0].legend(fontsize=8)
    fig.suptitle(f"evt {r['evt']}: track zoom"); fig.tight_layout(); fig.savefig(f"{out}_evt{r['evt']}_zoom.png", dpi=80); plt.close(fig)
