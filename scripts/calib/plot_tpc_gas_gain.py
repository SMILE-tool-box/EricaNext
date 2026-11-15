#!/usr/bin/env python3
"""Gas gain maps of the TPC from a gain map file (anode_ch cathode_ch Ka Kb).

    /usr/bin/python3 scripts/calib/plot_tpc_gas_gain.py gain_map.dat -o <dir> [--group 2] [--energy 30.85]

Gas gain (Calamity Gain.c, EricaNext_try GainMap):
    G = Q(E) x W / E x 1 / e x 1 / G_board,   Q(E) = (E - Kb) / Ka   [ADU x sample]
Q is converted to coulomb with ADC_to_mV x Clock_to_nsec / R (SMILE-2+ CalcFADC: 2 mV x 20 ns / 50 ohm = 0.8 pC
per ADU x sample). W = 23.6 eV and G_board = 700 (ASIC of the readout board) are the SMILE-2+ values; whether
they hold for SMILE-3 is not checked (all gains scale with them).
Pairs with Ka = Kb = 0 are not calibrated. --group 2 / 4 shows the median over 2-channel groups / boards.
Writes <dir>/gas_gain_<label>_upic<p>.png (one per μ-PIC with calibrated pairs) and <dir>/gas_gain_<label>.csv.
"""
import argparse
import os

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

E_CHARGE = 1.602176634e-19   # [C]


def read_gain_map(path):
    """-> {(anode_ch, cathode_ch): (Ka, Kb)} of the calibrated pairs (Ka != 0)"""
    out = {}
    for line in open(path):
        f = line.split()
        if len(f) != 4 or line.lstrip().startswith("#"):
            continue
        a, c, ka, kb = int(f[0]), int(f[1]), float(f[2]), float(f[3])
        if ka != 0:
            out[a, c] = (ka, kb)
    return out


def gas_gain(ka, kb, energy_kev, w_ev, g_board, pc_per_unit):
    q = (energy_kev - kb) / ka                       # [ADU x sample]
    q_c = q * pc_per_unit * 1e-12                    # [C]
    return q_c * (w_ev * 1e-3 / energy_kev) / E_CHARGE / g_board


def plot_gas_gain(dat, outdir, group=2, energy=30.85, w_ev=23.6, g_board=700.0,
                  adc_to_mv=2.0, clock_ns=20.0, r_ohm=50.0, show=False):
    """Draw the maps; returns (list of png paths, {(anode group, cathode group): gain})"""
    pc_per_unit = adc_to_mv * clock_ns / r_ohm          # mV x ns / ohm = pC
    pairs = read_gain_map(dat)
    per = {}
    for (a, c), (ka, kb) in pairs.items():
        per.setdefault((a // group, c // group), []).append(gas_gain(ka, kb, energy, w_ev, g_board, pc_per_unit))
    gains = {k: float(np.median(v)) for k, v in per.items()}
    label = {1: "ch", 2: "2ch", 4: "board"}[group]
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, f"gas_gain_{label}.csv"), "w") as f:
        f.write(f"# gas gain at {energy:g} keV from {os.path.abspath(dat)}; W {w_ev} eV, G_board {g_board}, "
                f"{pc_per_unit:g} pC/(ADU x sample); median over {group}-channel groups\n")
        f.write(f"anode_{label},cathode_{label},gain,n_pairs\n")
        for (a, c), g in sorted(gains.items()):
            f.write(f"{a},{c},{g:.1f},{len(per[a, c])}\n")
    pngs = []
    if not gains:
        return pngs, gains
    n = 12 // group
    vmin, vmax = np.percentile(list(gains.values()), [2, 98])
    for panel in range(4):
        z = np.full((n, n), np.nan)
        for (a, c), g in gains.items():
            if a // n == panel and c // n == panel:
                z[c - n * panel, a - n * panel] = g
        if np.isnan(z).all():
            continue
        fig, ax = plt.subplots(figsize=(8.5, 7), layout="constrained")
        m = ax.imshow(np.ma.masked_invalid(z), origin="lower", cmap="viridis", vmin=vmin, vmax=vmax)
        fig.colorbar(m, ax=ax, label="gas gain")
        for (i, j), g in np.ndenumerate(z):
            if np.isfinite(g):
                ax.text(j, i, f"{g:.0f}", ha="center", va="center", fontsize=9 if n <= 6 else 6,
                        color="w" if g < (vmin + vmax) / 2 else "k")
        per_board = 4 // group if group < 4 else 1
        for b in range(per_board, n, per_board):    # board boundaries
            ax.axvline(b - 0.5, color="k", ls="--", lw=2)
            ax.axhline(b - 0.5, color="k", ls="--", lw=2)
        ax.set_xticks(range(n)); ax.set_yticks(range(n))
        boards = [3 * panel + k for k in range(3)]
        sec = ax.secondary_xaxis("bottom"); sec.spines["bottom"].set_position(("outward", 22))
        sec.set_xticks([per_board * k + (per_board - 1) / 2 for k in range(3)], [f"Anode {b}" for b in boards])
        sec.tick_params(length=0)
        sec_y = ax.secondary_yaxis("left"); sec_y.spines["left"].set_position(("outward", 22))
        sec_y.set_yticks([per_board * k + (per_board - 1) / 2 for k in range(3)], [f"Cathode {b}" for b in boards],
                         rotation=90, va="center")
        sec_y.tick_params(length=0)
        ax.set_title(f"μPIC{panel}: gas gain at {energy:g} keV ({label}; 0 = centre side; white: not calibrated)")
        png = os.path.join(outdir, f"gas_gain_{label}_upic{panel}.png")
        fig.savefig(png, dpi=120)
        if show:
            plt.show()
        plt.close(fig)
        pngs.append(png)
    return pngs, gains


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("gain_map", help="gain map file: anode_ch cathode_ch Ka [keV/(ADU x sample)] Kb [keV]")
    ap.add_argument("-o", "--outdir", required=True)
    ap.add_argument("--group", type=int, choices=(1, 2, 4), default=2, help="channels per cell: 1 ch, 2 ch, board (4)")
    ap.add_argument("--energy", type=float, default=30.85, help="energy at which Q(E) = (E - Kb) / Ka is taken [keV]")
    ap.add_argument("--w-ev", type=float, default=23.6, help="W value of the gas [eV] (SMILE-2+ value)")
    ap.add_argument("--g-board", type=float, default=700.0, help="gain of the readout board ASIC (SMILE-2+ value)")
    ap.add_argument("--adc-to-mv", type=float, default=2.0, help="FADC [mV/ADU] (SMILE-2+ CalcFADC)")
    ap.add_argument("--clock-ns", type=float, default=20.0, help="FADC sample [ns]")
    ap.add_argument("--r-ohm", type=float, default=50.0, help="input impedance [ohm]")
    args = ap.parse_args()
    pngs, gains = plot_gas_gain(args.gain_map, args.outdir, args.group, args.energy, args.w_ev, args.g_board,
                                args.adc_to_mv, args.clock_ns, args.r_ohm)
    if gains:
        v = list(gains.values())
        print(f"gas gain: median {np.median(v):.0f}, {min(v):.0f}-{max(v):.0f} ({len(v)} cells)")
    for p in pngs:
        print("wrote", p)


if __name__ == "__main__":
    main()
