#!/usr/bin/env python3
"""Figure of the positions used by the reconstruction, in EricaNext coordinates.

    /usr/bin/python3 scripts/calib/plot_detector_coordinates.py calib/psa/geometry.csv -o calib/detector_coordinates

Writes <out>/coordinates.png: the μ-PIC (Z = 0), the GEM (Z = Z_GEM) and the drift cage top plate (Z = Z_TOP)
as planes over the μ-PIC area, and the absorption point of every HA scintillator pixel from geometry.csv
(x0, y0, z0) + pitch × ((n − 4.5) u + (m − 4.5) v), n, m = 1..8.
EricaNext coordinates: origin = centre of the μ-PIC, X: W -> E, Y: N -> S, Z: downward.
"""
import argparse
import os

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

Z_GEM = 10.0      # [mm] GEM from the μ-PIC (CAD; TOT_Skewness.z_gem_mm)
Z_TOP = 317.0     # [mm] inner face of the drift cage top plate (CAD)
HALF = 153.6      # [mm] half width of the μ-PIC (2 x 384 strips x 0.4 mm)
PLANES = [(0.0, "μ-PIC", "tab:red"), (Z_GEM, "GEM", "tab:orange"), (Z_TOP, "top plate", "tab:blue")]


def read_geometry(path):
    rows = []
    with open(path) as f:
        lines = [l for l in f if l.strip() and not l.startswith("#")]
    head = lines[0].strip().split(",")
    for l in lines[1:]:
        rows.append(dict(zip(head, map(float, l.strip().split(",")))))
    return rows


def pixels(rows):
    """-> {board: array (n_pixels, 3)} of the pixel absorption points"""
    nm = np.array([(n, m) for n in range(1, 9) for m in range(1, 9)], float) - 4.5
    out = {}
    for r in rows:
        c = np.array([r["x0"], r["y0"], r["z0"]])
        u = np.array([r["ux"], r["uy"], r["uz"]]); v = np.array([r["vx"], r["vy"], r["vz"]])
        p = c + r["pitch"] * (nm[:, :1] * u + nm[:, 1:] * v)
        out.setdefault(int(r["board"]), []).append(p)
    return {b: np.concatenate(v) for b, v in out.items()}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("geometry", help="calib/psa/geometry.csv")
    ap.add_argument("-o", "--outdir", required=True)
    args = ap.parse_args()
    pix = pixels(read_geometry(args.geometry))
    bottom = {b: p for b, p in pix.items() if np.ptp(p[:, 2]) < 1}       # a flat block = bottom HA
    colour = {b: ("tab:green" if b in bottom else "tab:olive") for b in pix}

    fig = plt.figure(figsize=(15, 13), layout="constrained")
    gs = fig.add_gridspec(2, 2)
    ax3 = fig.add_subplot(gs[0, 0], projection="3d")
    axt, axy, axx = fig.add_subplot(gs[0, 1]), fig.add_subplot(gs[1, 0]), fig.add_subplot(gs[1, 1])

    # 3D
    sq = np.array([[-1, -1], [1, -1], [1, 1], [-1, 1], [-1, -1]]) * HALF
    for z, name, c in PLANES:
        ax3.plot(sq[:, 0], sq[:, 1], z, color=c, lw=1.5)
        ax3.text(HALF, HALF, z, f" {name}  Z = {z:g}", color=c, fontsize=9, va="bottom" if z == 0 else "top")
    for b, p in pix.items():
        ax3.scatter(p[:, 0], p[:, 1], p[:, 2], s=0.5, color=colour[b])
    ax3.set(xlabel="X [mm] (W→E)", ylabel="Y [mm] (N→S)", zlabel="Z [mm] (down)")
    ax3.view_init(elev=20, azim=-60); ax3.invert_zaxis()
    ax3.set_title("3D")

    # top view X-Y (from above: X right = east, Y down = south)
    for b, p in pix.items():
        if b in bottom:
            axt.scatter(p[:, 0], p[:, 1], s=1, color=colour[b])
            cx, cy = p[:, 0].mean(), p[:, 1].mean()
            axt.text(cx, cy, f"HA{b:02d}", ha="center", va="center", fontsize=9, fontweight="bold",
                     bbox=dict(fc="w", ec="none", alpha=0.7, pad=1))
        else:
            axt.scatter(p[:, 0], p[:, 1], s=1, color=colour[b])
            cx, cy = p[:, 0].mean(), p[:, 1].mean()
            r = np.hypot(cx, cy); k = (r + 25) / r if r else 1
            axt.text(cx * k, cy * k, f"HA{b:02d}", ha="center", va="center", fontsize=8)
    axt.add_patch(plt.Rectangle((-HALF, -HALF), 2 * HALF, 2 * HALF, fill=False, ec="tab:red", lw=1.5))
    axt.text(-HALF, -HALF - 6, "μ-PIC (GEM, top plate)", color="tab:red", fontsize=9, va="bottom")
    axt.invert_yaxis()
    axt.set(xlabel="X [mm] (W → E)", ylabel="Y [mm] (N → S)", title="top view X-Y (seen from above; north up)")

    # side views
    for ax, k, lab, title in [(axy, 1, "Y [mm] (N → S)", "side view Y-Z (seen from the west)"),
                              (axx, 0, "X [mm] (W → E)", "side view X-Z (seen from the south)")]:
        for b, p in pix.items():
            ax.scatter(p[:, k], p[:, 2], s=1, color=colour[b])
        for z, name, c in PLANES:
            ax.plot([-HALF, HALF], [z, z], color=c, lw=2)
            ax.text(HALF + 8, z, f"{name}  Z = {z:g}", color=c, fontsize=9, va="bottom" if z == 0 else "top")
        ax.invert_yaxis()
        ax.set(xlabel=lab, ylabel="Z [mm] (down)", title=title)

    for ax in (axt, axy, axx):
        ax.set_aspect("equal"); ax.grid(alpha=0.3)
    axy.set_xlim(-260, 400); axx.set_xlim(-260, 400); axt.set_xlim(-235, 235); axt.set_ylim(235, -235)
    handles = [plt.Line2D([], [], ls="", marker="o", color=c, label=l)
               for c, l in [("tab:green", "HA pixel (bottom)"), ("tab:olive", "HA pixel (side)")]]
    axt.legend(handles=handles, loc="lower right", fontsize=8)
    fig.suptitle("EricaNext coordinates: origin = centre of the μ-PIC, X: W → E, Y: N → S, Z: down\n"
                 "μ-PIC / GEM / top plate Z and the absorption points of all HA pixels (calib/psa/geometry.csv)")
    os.makedirs(args.outdir, exist_ok=True)
    png = os.path.join(args.outdir, "coordinates.png")
    fig.savefig(png, dpi=100); plt.close(fig)
    print("wrote", png)


if __name__ == "__main__":
    main()
