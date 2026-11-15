#!/usr/bin/env python3
"""Figures of the SMILE-3 inner assembly (CAD STEP) and the EricaNext detector coordinates.

    /usr/bin/python3 scripts/calib/plot_detector_coordinates.py \
        calib/detector_coordinates/SM3_Inner_3atm.step -o calib/detector_coordinates

Writes <out>/cad_view.png (the vertex points of every CAD part, side and top views, in CAD coordinates)
and <out>/coordinates.png (the same parts in EricaNext coordinates with both coordinate systems drawn).
EricaNext: origin = centre of the μ-PIC (lower face of its substrate), X = y_CAD, Y = x_CAD,
Z = Z_UPIC_CAD - z_CAD (downwards, into the drift volume). No CAD library: step_reader.py reads the STEP.
"""
import argparse
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from step_reader import Step  # noqa: E402

Z_UPIC_CAD = 427.04   # [mm] lower face of the μ-PIC substrate in CAD z (= EricaNext Z = 0)

# (label, colour, test on the assembly path); the first match wins
GROUPS = [
    ("μ-PIC", "tab:red", lambda p: "uPIC" in p),
    ("GEM", "tab:orange", lambda p: "GEM" in p),
    ("drift cage", "tab:blue", lambda p: "Drift_" in p),
    ("HA (bottom)", "tab:green", lambda p: "Bottom_1_unit" in p),
    ("HA (side)", "tab:olive", lambda p: "Side_1_unit" in p),
    ("other (frames, TPC parts, TCU)", "0.6", lambda p: True),
]


def load(step_path):
    st = Step(step_path)
    st.build()
    pts = {g[0]: [] for g in GROUPS}
    for path, _, _, w in st.walk():
        if not len(w):
            continue
        for label, _, test in GROUPS:
            if test(path):
                pts[label].append(w)
                break
    return {k: np.concatenate(v) for k, v in pts.items() if v}


def scatter(ax, pts, fx, fy):
    for label, colour, _ in GROUPS[::-1]:          # draw "other" first, the detectors on top
        if label in pts:
            p = pts[label]
            ax.scatter(fx(p), fy(p), s=0.3, color=colour, label=label, rasterized=True)


def to_erica(p):
    return np.column_stack([p[:, 1], p[:, 0], Z_UPIC_CAD - p[:, 2]])


def arrow(ax, origin, d, text, colour, length=120):
    o = np.asarray(origin, float); d = np.asarray(d, float)
    ax.annotate("", xy=o + length * d, xytext=o, arrowprops=dict(arrowstyle="->", lw=2, color=colour))
    ax.text(*(o + (length + 18) * d), text, color=colour, fontsize=11, ha="center", va="center", fontweight="bold")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("step", help="SM3_Inner_3atm.step")
    ap.add_argument("-o", "--outdir", required=True)
    args = ap.parse_args()
    pts = load(args.step)
    os.makedirs(args.outdir, exist_ok=True)

    # 1. CAD view in CAD coordinates
    fig, axs = plt.subplots(1, 2, figsize=(14, 7.5), layout="constrained")
    scatter(axs[0], pts, lambda p: p[:, 0], lambda p: p[:, 2])
    axs[0].set(xlabel="x_CAD [mm]", ylabel="z_CAD [mm] (up)", title="side view (CAD x-z)")
    scatter(axs[1], pts, lambda p: p[:, 0], lambda p: p[:, 1])
    axs[1].set(xlabel="x_CAD [mm]", ylabel="y_CAD [mm]", title="top view (CAD x-y, seen from +z)")
    for ax in axs:
        ax.set_aspect("equal"); ax.grid(alpha=0.3)
        arrow(ax, (0, 0), (1, 0), "x", "k", 80)
    arrow(axs[0], (0, 0), (0, 1), "z", "k", 80)
    arrow(axs[1], (0, 0), (0, 1), "y", "k", 80)
    axs[0].legend(markerscale=20, fontsize=8, loc="lower right")
    fig.suptitle("SM3_Inner_3atm.step: vertex points of every part (CAD coordinates, origin = bottom plate)")
    fig.savefig(os.path.join(args.outdir, "cad_view.png"), dpi=110); plt.close(fig)

    # 2. EricaNext coordinates
    e = {k: to_erica(v) for k, v in pts.items()}
    fig, axs = plt.subplots(1, 2, figsize=(14, 7.5), layout="constrained")
    scatter(axs[0], e, lambda p: p[:, 1], lambda p: p[:, 2])
    axs[0].invert_yaxis()
    axs[0].set(xlabel="Y [mm] (= x_CAD)", ylabel="Z [mm] (= 427.04 − z_CAD, downwards)",
               title="side view: EricaNext Y-Z")
    for z, text, ha in [(0, "μ-PIC  Z = 0", "left"), (10, "GEM  Z = 10", "right"),
                        (317, "drift cage top plate  Z = 317", "left")]:
        axs[0].axhline(z, color="k", lw=0.6, ls="--")
        x0, x1 = axs[0].get_xlim()
        axs[0].text(x0 + 10 if ha == "left" else x1 - 10, z + (-4 if ha == "left" else 16), text,
                    fontsize=9, ha=ha, va="bottom")
    arrow(axs[0], (0, 0), (1, 0), "Y", "tab:purple")
    arrow(axs[0], (0, 0), (0, 1), "Z", "tab:purple")
    scatter(axs[1], e, lambda p: p[:, 1], lambda p: p[:, 0])
    axs[1].set(xlabel="Y [mm] (= x_CAD)", ylabel="X [mm] (= y_CAD)",
               title="top view: EricaNext X-Y (seen from −Z, i.e. from above)")
    arrow(axs[1], (0, 0), (1, 0), "Y", "tab:purple")
    arrow(axs[1], (0, 0), (0, 1), "X", "tab:purple")
    for ax in axs:
        ax.set_aspect("equal"); ax.grid(alpha=0.3)
    axs[1].legend(markerscale=20, fontsize=8, loc="lower right")
    fig.suptitle("EricaNext coordinates: origin = centre of the μ-PIC, X = y_CAD, Y = x_CAD, Z = 427.04 − z_CAD")
    fig.savefig(os.path.join(args.outdir, "coordinates.png"), dpi=110); plt.close(fig)
    print("wrote", os.path.join(args.outdir, "cad_view.png"), os.path.join(args.outdir, "coordinates.png"))


if __name__ == "__main__":
    main()
