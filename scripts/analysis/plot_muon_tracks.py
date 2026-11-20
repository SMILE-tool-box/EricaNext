#!/usr/bin/env python3
"""Straight, long TPC tracks (muon-like) with the HA hits of the same event, one PNG per event.

    python3 run_script/run_chain.py smile3_reconstruct --root-input <etcc raw.root> -o recon_cloud.root \\
        --first-entry 2000000 -n 20000 --set TOT_Skewness.flag_save_cloud=true
    /usr/bin/python3 scripts/analysis/plot_muon_tracks.py recon_cloud.root calib/psa/geometry.csv <out prefix> \\
        [--min-extent 200 --max-rms 5 --top 8 --min-ha-energy 0]

- Input: a smile3_reconstruct output made with TOT_Skewness.flag_save_cloud=true. TOT_Skewness/tot_skewness must
  have evt_id (from commit 7f3c522); the HA hits come from PSA_TreeWriter/psa, matched by trig == evt_id.
- A track is the TOT_Skewness point cloud (unique points). Straight-line fit by PCA: extent = 1-99 % range of the
  points along the line, rms = RMS distance of the points from the line. Kept: >= --min-points points,
  extent >= --min-extent mm, rms <= --max-rms mm; the longest first.
- HA hits: hits with a pixel, at the position of PSA_Event (centre of the block + pitch ((n - 4.5) u + (m - 4.5) v)
  with geometry.csv), with energy >= --min-ha-energy keV.
- Drawn: 3D and the X-Y, X-Z, Y-Z projections; track points (blue), the fitted line (dotted), TOT_Skewness Pe and
  Ve (orange), HA hits (green squares, energy [keV] written in X-Y; saturated hits: open purple squares,
  ">E sat HG" / ">E sat HG+LG": their energy is a lower limit and their pixel is pulled to the block centre), the TPC drift volume (uPIC +-153.6 mm,
  Z 5-317 mm) and the HA blocks of geometry.csv. Z is drawn downwards (geometry.csv: X: E, Y: S, Z: down).
- The track is in the frame of TOT_Skewness and the HA in that of geometry.csv; that the two are the same is not
  checked yet (AGENTS.md).
- Needs PyROOT, numpy, matplotlib: /usr/bin/python3.
"""
import argparse
import csv

import numpy as np

HX, ZG, ZT = 153.6, 5.0, 317.0      # uPIC half width, GEM (TOT_Skewness.z_gem_mm) and top of the drift [mm]
HIT_STYLE = dict(c="#2b8a3e", edgecolors="#2b8a3e")                       # HA hit: filled green square
SAT_STYLE = dict(c="white", edgecolors="#7048e8", linewidths=1.5)          # saturated HA hit: open purple square


def read_geometry(path):
    """{(board, ch): block} of geometry.csv"""
    rows = csv.DictReader(l for l in open(path) if not l.startswith("#"))
    return {(int(float(r["board"])), int(float(r["ch"]))): {k: float(v) for k, v in r.items()} for r in rows}


def block_corners(b):
    c = np.array([b["x0"], b["y0"], b["z0"]]); u = np.array([b["ux"], b["uy"], b["uz"]]); v = np.array([b["vx"], b["vy"], b["vz"]])
    h = 4 * b["pitch"]
    return np.array([c + su * h * u + sv * h * v for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1), (-1, -1))])


def read_ha_hits(f, geo, min_energy=0.0):
    """{trig: [(position [mm], energy [keV], board, saturation)]} of the PSA hits with a pixel. saturation: "" (none),
    "HG" (high gain saturated), "HG+LG" (both) or "LG" (low gain only).
    A saturated hit has its centre of gravity pulled to the middle of the block (HG: about 1/3 of the true offset,
    HG+LG: almost 0), so its pixel is not where it hit, and its energy is a lower limit."""
    hits = {}
    for h in f.Get("PSA_TreeWriter/psa"):
        b = geo.get((h.board, h.ch))
        if h.pixel < 0 or b is None or h.energy < min_energy:
            continue
        du, dv = b["pitch"] * (h.pix_n - 4.5), b["pitch"] * (h.pix_m - 4.5)
        pos = np.array([b["x0"] + du * b["ux"] + dv * b["vx"], b["y0"] + du * b["uy"] + dv * b["vy"],
                        b["z0"] + du * b["uz"] + dv * b["vz"]])
        sat = ("HG+LG" if h.satu_l else "HG") if h.satu_h else ("LG" if h.satu_l else "")
        hits.setdefault(int(h.trig), []).append((pos, float(h.energy), int(h.board), sat))
    return hits


def find_straight_tracks(f, min_points=50, min_extent=200.0, max_rms=5.0):
    """[dict(evt, points, centre, axis, extent, rms, Pe, Ve)] of the straight long tracks, the longest first"""
    out = []
    for t in f.Get("TOT_Skewness/tot_skewness"):
        if t.Pe[0] > 1e8 or len(t.cloud_x) < min_points:
            continue
        p = np.unique(np.column_stack([np.array(t.cloud_x), np.array(t.cloud_y), np.array(t.cloud_z)]), axis=0)
        if len(p) < min_points:
            continue
        m = p.mean(0); c = p - m
        w, v = np.linalg.eigh(np.cov(c.T)); ax = v[:, -1]
        proj = c @ ax
        extent = np.percentile(proj, 99) - np.percentile(proj, 1)
        rms = np.sqrt(np.mean(np.sum((c - np.outer(proj, ax)) ** 2, axis=1)))
        if extent >= min_extent and rms <= max_rms:
            out.append(dict(evt=int(t.evt_id), points=p, centre=m, axis=ax, extent=extent, rms=rms,
                            Pe=np.array(t.Pe), Ve=np.array(t.Ve)))
    return sorted(out, key=lambda r: -r["extent"])


def plot_track(r, ha_hits, geo, path):
    """one event: 3D + X-Y, X-Z, Y-Z projections; returns the figure path"""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    p, m, ax, Pe, Ve = r["points"], r["centre"], r["axis"], r["Pe"], r["Ve"]
    L = r["extent"] * 0.6
    zen = np.degrees(np.arccos(abs(ax[2])))
    fig = plt.figure(figsize=(20, 6.5))
    a3 = fig.add_subplot(1, 4, 1, projection="3d")
    for zz in (ZG, ZT):
        a3.plot([-HX, HX, HX, -HX, -HX], [-HX, -HX, HX, HX, -HX], zz, color="#bbbbbb", lw=0.6)
    for b in geo.values():
        cc = block_corners(b); a3.plot(cc[:, 0], cc[:, 1], cc[:, 2], color="#dddddd", lw=0.4)
    a3.scatter(p[:, 0], p[:, 1], p[:, 2], s=3, c="#2a6fdb")
    a3.plot(*[[m[k] - L * ax[k], m[k] + L * ax[k]] for k in range(3)], color="#666666", lw=1, ls=":")
    a3.scatter(*Pe, s=60, c="#e8590c", marker="*")
    for pos, e, bd, sat in ha_hits:
        a3.scatter(*pos, s=50, marker="s", **(SAT_STYLE if sat else HIT_STYLE))
    a3.set(xlabel="X [mm]", ylabel="Y [mm]", zlabel="Z [mm]"); a3.invert_zaxis()
    ve = Ve / np.linalg.norm(Ve)
    for k, (an, bn, i, j) in enumerate((("x", "y", 0, 1), ("x", "z", 0, 2), ("y", "z", 1, 2))):
        g = fig.add_subplot(1, 4, k + 2)
        if (i, j) == (0, 1):
            g.plot([-HX, HX, HX, -HX, -HX], [-HX, -HX, HX, HX, -HX], color="#bbbbbb")
        else:
            g.plot([-HX, HX, HX, -HX, -HX], [ZG, ZG, ZT, ZT, ZG], color="#bbbbbb")
        for b in geo.values():
            cc = block_corners(b); g.plot(cc[:, i], cc[:, j], color="#e4e4e4", lw=0.5)
        g.scatter(p[:, i], p[:, j], s=3, c="#2a6fdb", label="track points")
        labelled = set()
        for pos, e, bd, sat in ha_hits:
            style = SAT_STYLE if sat else HIT_STYLE
            lab = ("HA hits, saturated (pixel pulled to the block centre)" if sat else "HA hits (pixel)")
            g.scatter(pos[i], pos[j], s=45, marker="s", zorder=6, label=None if lab in labelled else lab, **style)
            labelled.add(lab)
            if (i, j) == (0, 1):
                g.annotate(f">{e:.0f} sat {sat}" if sat else f"{e:.0f}", (pos[i], pos[j]), fontsize=7,
                           color=style["edgecolors"], xytext=(4, 3), textcoords="offset points")
        g.plot([m[i] - L * ax[i], m[i] + L * ax[i]], [m[j] - L * ax[j], m[j] + L * ax[j]], color="#666666", lw=1, ls=":",
               label="straight-line fit (PCA)")
        g.scatter(Pe[i], Pe[j], s=80, c="#e8590c", marker="*", label="TOT_Skewness Pe", zorder=5)
        g.plot([Pe[i], Pe[i] + 40 * ve[i]], [Pe[j], Pe[j] + 40 * ve[j]], color="#e8590c", lw=2, label="Ve (40 mm)")
        g.set(xlabel=f"{an.upper()} [mm]", ylabel=f"{bn.upper()} [mm]", aspect="equal", xlim=(-210, 210),
              ylim=((-210, 210) if j == 1 else (380, -40)))
    fig.axes[1].legend(fontsize=7, loc="upper left")
    z = p[:, 2]
    n_sat = sum(1 for h in ha_hits if h[3])
    fig.suptitle(f"evt {r['evt']}: {len(ha_hits)} HA hits ({n_sat} saturated; energy [keV] in X-Y, '>' = saturated), straight track {r['extent']:.0f} mm, "
                 f"rms {r['rms']:.1f} mm from the line, zenith {zen:.0f} deg, z {z.min():.0f}-{z.max():.0f} mm (drift {ZG:g}-{ZT:g} mm)")
    fig.tight_layout(); fig.savefig(path, dpi=80); plt.close(fig)
    return path


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("root"); ap.add_argument("geometry"); ap.add_argument("out")
    ap.add_argument("--min-points", type=int, default=50)
    ap.add_argument("--min-extent", type=float, default=200.0, help="[mm]")
    ap.add_argument("--max-rms", type=float, default=5.0, help="[mm]")
    ap.add_argument("--min-ha-energy", type=float, default=0.0, help="[keV]")
    ap.add_argument("--top", type=int, default=8)
    a = ap.parse_args()
    import ROOT
    ROOT.gROOT.SetBatch(True)
    f = ROOT.TFile.Open(a.root)
    geo = read_geometry(a.geometry)
    hits = read_ha_hits(f, geo, a.min_ha_energy)
    tracks = find_straight_tracks(f, a.min_points, a.min_extent, a.max_rms)
    print(f"straight tracks (extent >= {a.min_extent:g} mm, rms <= {a.max_rms:g} mm): {len(tracks)}")
    for r in tracks[:a.top]:
        h = hits.get(r["evt"], [])
        path = plot_track(r, h, geo, f"{a.out}_evt{r['evt']}.png")
        print(f"evt {r['evt']}: extent {r['extent']:.0f} mm, rms {r['rms']:.1f} mm, HA hits {len(h)} "
              f"({', '.join(f'>{e:.0f} sat' if sat else f'{e:.0f}' for _, e, _, sat in h)} keV) -> {path}")


if __name__ == "__main__":
    main()
