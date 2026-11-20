#!/usr/bin/env python3
"""Figure of the positions used by the reconstruction, in EricaNext coordinates.

    /usr/bin/python3 scripts/calib/plot_detector_coordinates.py calib/psa/geometry.csv -o calib/detector_coordinates

Writes <out>/coordinates.png: the μ-PIC (Z = 0), the GEM (Z = Z_GEM) and the drift cage top plate (Z = Z_TOP)
as planes over the μ-PIC area, and the absorption point of every HA scintillator pixel from geometry.csv
(x0, y0, z0) + pitch × ((n − 4.5) u + (m − 4.5) v), n, m = 1..8.
Writes <out>/ha_layout.png: which block is which HA and ch in geometry.csv, drawn like the HA layout figure: seen from
above (X right = E, Y down = S) with the side walls unfolded outward about their bottom edges (so their inner faces
are seen); every block labelled "HAnn chc", the corner of pixel (1, 1) marked and arrows for increasing n (u) and m (v).
Writes <out>/ha_views.png: the same labels in 5 views, the bottom HAs seen from above (X to the right, Y
downward) and each side wall seen from outside, from N, S, W and E (Z down in the views of the walls).
EricaNext coordinates: origin = centre of the μ-PIC, X: W -> E, Y: N -> S, Z: downward.
"""
import argparse
import os

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

Z_GEM = 5.0       # [mm] GEM from the μ-PIC (TOT_Skewness.z_gem_mm; 10 mm with the second GEM, as in the CAD)
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


def ha_layout(rows, path):
    """2D map of the blocks: bottom from above, walls unfolded outward about their bottom edges"""
    zb = max(r["z0"] for r in rows if abs(r["x0"]) > 170 or abs(r["y0"]) > 170) + 27.0   # bottom edge of the walls
    R = 200.0                                                                              # where the edge is drawn

    def flat(p):
        x, y, z = p
        if abs(x) > 170: return np.array([np.sign(x) * (R + (zb - z)), y])
        if abs(y) > 170: return np.array([x, np.sign(y) * (R + (zb - z))])
        return np.array([x, y])

    fig, ax = plt.subplots(figsize=(14, 14), layout="constrained")
    for r in rows:
        c = np.array([r["x0"], r["y0"], r["z0"]])
        u = np.array([r["ux"], r["uy"], r["uz"]]); v = np.array([r["vx"], r["vy"], r["vz"]]); h = 4 * r["pitch"]
        # the wall blocks: shift the centre to the inner face is not needed for the map (same 2D place)
        corners = [flat(c + h * (su * u + sv * v)) for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1), (-1, -1))]
        cc = np.array(corners); bottom = abs(r["x0"]) < 170 and abs(r["y0"]) < 170
        ax.fill(cc[:, 0], cc[:, 1], color="#cfe8d4" if bottom else "#e6e8c8", ec="#555555", lw=0.8)
        c2 = flat(c)
        ax.text(c2[0], c2[1], f"HA{int(r['board']):02d}\nch{int(r['ch'])}", ha="center", va="center", fontsize=7.5,
                fontweight="bold" if int(r["ch"]) == 0 else None)
        p11 = flat(c + r["pitch"] * (-3.5 * u - 3.5 * v))
        ax.plot(*p11, "o", color="tab:red", ms=4)
        for vec, col in ((u, "tab:blue"), (v, "tab:orange")):
            a = flat(c + r["pitch"] * (-3.5 * u - 3.5 * v)); b = flat(c + r["pitch"] * (-3.5 * u - 3.5 * v) + 14 * vec)
            ax.annotate("", b, a, arrowprops=dict(arrowstyle="->", color=col, lw=1.2))
    ax.add_patch(plt.Rectangle((-HALF, -HALF), 2 * HALF, 2 * HALF, fill=False, ec="tab:red", lw=1, ls="--"))
    for t, (x, y) in {"N": (0, -R - 175), "S": (0, R + 175), "W": (-R - 175, 0), "E": (R + 175, 0)}.items():
        ax.text(x, y, t, fontsize=18, fontweight="bold", ha="center", va="center", color="#444444")
    ax.invert_yaxis(); ax.set_aspect("equal"); ax.grid(alpha=0.2)
    ax.set(xlabel="X [mm] (W → E)", ylabel="Y [mm] (N → S)", xlim=(-400, 400), ylim=(400, -400))
    handles = [plt.Line2D([], [], ls="", marker="o", color="tab:red", label="pixel (1, 1)"),
               plt.Line2D([], [], color="tab:blue", label="n increases (u)"),
               plt.Line2D([], [], color="tab:orange", label="m increases (v)")]
    ax.legend(handles=handles, loc="upper left", fontsize=9)
    ax.set_title("HA blocks of geometry.csv, seen from above (Z into the page); side walls unfolded outward about their "
                 "bottom edges\n(their inner faces are seen; outer = top of the wall). Dashed: μ-PIC. Compare with the HA layout figure.")
    fig.savefig(path, dpi=100); plt.close(fig)
    print("wrote", path)


VIEWS = {   # name: (wall selector, horizontal axis = (vector, label), vertical axis, title)
    "top": (lambda c: abs(c[0]) < 170 and abs(c[1]) < 170, ((1, 0, 0), "X [mm]  (W → E)"), ((0, 1, 0), "Y [mm]  (N → S, down)"),
            "bottom HAs seen from above (Z into the page; X to the right, Y downward: north at the top)"),
    "N": (lambda c: c[1] < -170, ((-1, 0, 0), "−X [mm]  (E → W)"), ((0, 0, 1), "Z [mm] (down)"),
          "north wall seen from outside (from the north, facing south)"),
    "S": (lambda c: c[1] > 170, ((1, 0, 0), "X [mm]  (W → E)"), ((0, 0, 1), "Z [mm] (down)"),
          "south wall seen from outside (from the south, facing north)"),
    "W": (lambda c: c[0] < -170, ((0, 1, 0), "Y [mm]  (N → S)"), ((0, 0, 1), "Z [mm] (down)"),
          "west wall seen from outside (from the west, facing east)"),
    "E": (lambda c: c[0] > 170, ((0, -1, 0), "−Y [mm]  (S → N)"), ((0, 0, 1), "Z [mm] (down)"),
          "east wall seen from outside (from the east, facing west)"),
}


def ha_views(rows, path):
    """bottom from above and each wall from outside; blocks labelled HA / ch, pixel (1, 1) and the n, m directions"""
    fig = plt.figure(figsize=(18, 17), layout="constrained")
    gs = fig.add_gridspec(3, 2, height_ratios=[1.6, 1, 1])
    axes = {"top": fig.add_subplot(gs[0, :]), "N": fig.add_subplot(gs[1, 0]), "S": fig.add_subplot(gs[1, 1]),
            "W": fig.add_subplot(gs[2, 0]), "E": fig.add_subplot(gs[2, 1])}
    for name, (sel, (hv, hl), (vv, vl), title) in VIEWS.items():
        ax = axes[name]; hv, vv = np.array(hv, float), np.array(vv, float)
        P = lambda q: np.array([q @ hv, q @ vv])
        for r in rows:
            c = np.array([r["x0"], r["y0"], r["z0"]])
            if not sel(c):
                continue
            u = np.array([r["ux"], r["uy"], r["uz"]]); v = np.array([r["vx"], r["vy"], r["vz"]]); h = 4 * r["pitch"]
            cc = np.array([P(c + h * (su * u + sv * v)) for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1), (-1, -1))])
            ax.fill(cc[:, 0], cc[:, 1], color="#cfe8d4" if name == "top" else "#e6e8c8", ec="#555555", lw=0.8)
            c2 = P(c)
            ax.text(c2[0], c2[1], f"HA{int(r['board']):02d}\nch{int(r['ch'])}", ha="center", va="center",
                    fontsize=9, fontweight="bold" if int(r["ch"]) == 0 else None)
            p11 = c + r["pitch"] * (-3.5 * u - 3.5 * v)
            ax.plot(*P(p11), "o", color="tab:red", ms=5)
            for vec, col in ((u, "tab:blue"), (v, "tab:orange")):
                ax.annotate("", P(p11 + 14 * vec), P(p11), arrowprops=dict(arrowstyle="->", color=col, lw=1.4))
        if name == "top":
            ax.add_patch(plt.Rectangle((-HALF, -HALF), 2 * HALF, 2 * HALF, fill=False, ec="tab:red", lw=1, ls="--"))
        ax.invert_yaxis()            # walls: Z downward; bottom from above: Y downward
        ax.set_aspect("equal"); ax.grid(alpha=0.2)
        ax.set(xlabel=hl, ylabel=vl, title=title)
    handles = [plt.Line2D([], [], ls="", marker="o", color="tab:red", label="pixel (1, 1)"),
               plt.Line2D([], [], color="tab:blue", label="n increases (u)"),
               plt.Line2D([], [], color="tab:orange", label="m increases (v)")]
    axes["top"].legend(handles=handles, loc="upper left", bbox_to_anchor=(1.02, 1), fontsize=10)
    fig.suptitle("HA blocks of geometry.csv: which HA and ch, pixel (1, 1) and the directions of n, m", fontsize=14)
    fig.savefig(path, dpi=90); plt.close(fig)
    print("wrote", path)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("geometry", help="calib/psa/geometry.csv")
    ap.add_argument("-o", "--outdir", required=True)
    args = ap.parse_args()
    geometry = read_geometry(args.geometry)
    pix = pixels(geometry)
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
    ha_layout(geometry, os.path.join(args.outdir, "ha_layout.png"))
    ha_views(geometry, os.path.join(args.outdir, "ha_views.png"))


if __name__ == "__main__":
    main()
