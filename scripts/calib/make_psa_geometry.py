#!/usr/bin/env python3
"""calib/psa/geometry.csv (PSA_Event) from the CAD STEP file of the SMILE-3 inner assembly.

    /usr/bin/python3 scripts/calib/make_psa_geometry.py calib/cad/SM3_Inner_3atm.step -o calib/psa/geometry.csv

Positions come from the CAD (the GSO_26mm / GSO_13mm crystal blocks); which HA board and ch each block is, and the
direction of the pixel numbers, come from the layout figure of the HA modules (table HA_LAYOUT below).

Coordinates (tutorials/0_Introduction): origin at the centre of the uPIC readout plane, X: W -> E, Y: N -> S,
Z: downward. CAD: z upward from the bottom plate; the uPIC board (TGV uPIC) has its lower face at z = 427.04 mm.
The CAD is symmetric in x and y, so CAD +y is taken as East:  X = y_cad,  Y = x_cad,  Z = Z_UPIC_CAD - z_cad
(right-handed). The CAD gives GEM at Z = 10.0 mm and the inner face of the drift top plate at Z = 317.0 mm.

Module frame (layout figure, inset of HA15): the 6 blocks are  0 1 / 2 3 / 4 5 ; "c" = direction ch0 -> ch1 and of
increasing pixel n (0-7 in the figure = n 1-8), "r" = direction ch0 -> ch2 -> ch4 and of increasing pixel m. Every HA
looks the same in the orientation it is drawn in the figure (its connector and head-amp bar); the side walls are drawn
unfolded outward about their bottom edges (arrows: top -> bottom), so
  bottom (HA0-5), seen from above:          c = N (-Y),  r = E (+X)
  side walls, seen from inside the TPC:     c = to the right,  r = down (+Z)
                                            (N wall c = +X, E wall c = +Y, S wall c = -X, W wall c = -Y)
PSA_Event: absorption point of pixel (n, m) = (x0, y0, z0) + pitch ((n - 4.5) u + (m - 4.5) v), with u = c, v = r and
(x0, y0, z0) = centre of the block (middle of the crystal depth).
The pixel directions also depend on the wiring of the 4 corner signals (sign of the centre-of-gravity formula):
check them with data before trusting them.
"""
import argparse
import datetime
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from step_reader import Step  # noqa: E402

Z_UPIC_CAD = 427.04       # lower face of the TGV uPIC board in the CAD [mm]
BLOCK_STEP = 54.0         # centre-to-centre distance of the blocks in a module [mm]
E = {"N": (0, -1, 0), "S": (0, 1, 0), "E": (1, 0, 0), "W": (-1, 0, 0), "down": (0, 0, 1)}

# HA: (approximate centre of the module (X, Y) [mm], c, r)  -- from the layout figure
HA_LAYOUT = {
    # bottom: west half HA3 / 4 / 5 (N -> S), east half HA0 / 1 / 2 (N -> S)
    0: ((81, -108), "N", "E"), 1: ((81, 0), "N", "E"), 2: ((81, 108), "N", "E"),
    3: ((-81, -108), "N", "E"), 4: ((-81, 0), "N", "E"), 5: ((-81, 108), "N", "E"),
    # north wall, W -> E: HA8, 7, 6
    8: ((-108, -183.5), "E", "down"), 7: ((0, -183.5), "E", "down"), 6: ((108, -183.5), "E", "down"),
    # west wall, N -> S: HA9, 10, 11
    9: ((-183.5, -108), "N", "down"), 10: ((-183.5, 0), "N", "down"), 11: ((-183.5, 108), "N", "down"),
    # south wall, W -> E: HA12, 13, 14
    12: ((-108, 183.5), "W", "down"), 13: ((0, 183.5), "W", "down"), 14: ((108, 183.5), "W", "down"),
    # east wall, N -> S: HA17, 16, 15
    17: ((183.5, -108), "S", "down"), 16: ((183.5, 0), "S", "down"), 15: ((183.5, 108), "S", "down"),
}


def to_det(p):
    """CAD -> detector coordinates"""
    p = np.asarray(p, dtype=float)
    return np.array([p[..., 1], p[..., 0], Z_UPIC_CAD - p[..., 2]]).T


def read_blocks(step_path):
    st = Step(step_path)
    st.build()
    blocks, pitch = [], []
    for path, name, m, w in st.walk():
        if not name.startswith("GSO") or not len(w):
            continue
        d = to_det(w)
        lo, hi = d.min(0), d.max(0)
        blocks.append(((lo + hi) / 2, hi - lo, name))
        # pixel pitch from the grooves: along a face the vertex coordinates come in 9 pairs 0.06 mm apart
        # (outer edge, 7 grooves, outer edge); the pitch is the distance between the starts of the pairs
        for ax in range(3):
            if hi[ax] - lo[ax] > 40:
                u = np.unique(np.round(d[:, ax], 2))
                if len(u) == 18:
                    pitch.append(float(np.mean(np.diff(u[0::2]))))
    return blocks, float(np.mean(pitch))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("step", help="STEP file of the inner assembly (SM3_Inner_3atm.step)")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--overwrite", action="store_true")
    args = ap.parse_args()
    if os.path.exists(args.output) and not args.overwrite:
        sys.exit(f"{args.output} exists (--overwrite to replace it)")

    blocks, pitch = read_blocks(args.step)
    centres = np.array([b[0] for b in blocks])
    print(f"{len(blocks)} crystal blocks, pixel pitch {pitch:.3f} mm")
    rows, used = [], set()
    for ha, ((x, y), c_name, r_name) in sorted(HA_LAYOUT.items()):
        c, r = np.array(E[c_name], float), np.array(E[r_name], float)
        # module centre: mean of the 6 blocks nearest to (x, y) in the plane
        dxy = np.hypot(centres[:, 0] - x, centres[:, 1] - y)
        if r_name == "down":     # side wall: blocks at any depth; the wall is at fixed X or Y
            near = np.argsort(dxy + 1e3 * (centres[:, 2] > 330))[:6]
        else:
            near = np.argsort(dxy + 1e3 * (centres[:, 2] < 330))[:6]
        mc = centres[near].mean(0)
        for ch in range(6):
            col, row = ch % 2, ch // 2
            want = mc + (col - 0.5) * BLOCK_STEP * c + (row - 1) * BLOCK_STEP * r
            k = int(np.argmin(np.linalg.norm(centres - want, axis=1)))
            dist = np.linalg.norm(centres[k] - want)
            if dist > 1.0 or k in used:
                sys.exit(f"HA{ha} ch{ch}: no CAD block at {np.round(want, 1)} (nearest {dist:.1f} mm)")
            used.add(k)
            p0 = centres[k]
            size = blocks[k][1]
            # the pixel plane must be the face of the block, the depth along the third axis
            if abs(size @ np.abs(c) - size @ np.abs(r)) > 0.5 or (size @ np.abs(c)) < 40:
                sys.exit(f"HA{ha} ch{ch}: c / r are not the face of the block (size {np.round(size, 2)})")
            rows.append((ha, ch, *p0, *c, *r, pitch))
    if len(used) != len(blocks):
        print(f"WARNING: {len(blocks) - len(used)} CAD blocks have no HA", file=sys.stderr)

    with open(args.output, "w") as f:
        f.write(f"# made by scripts/calib/make_psa_geometry.py, {datetime.datetime.now().isoformat(timespec='seconds')}\n")
        f.write(f"# input: {os.path.abspath(args.step)}\n")
        f.write("# PSA_Event geometry: one row per scintillator block (HA board, ch 0-5). Units: mm.\n")
        f.write("# Coordinates: origin at the centre of the uPIC, X: W -> E, Y: N -> S, Z: downward (Z = 427.04 - z_CAD,"
                " X = y_CAD, Y = x_CAD).\n")
        f.write("# (x0, y0, z0): centre of the 8x8 block (middle of the depth); u: increasing pixel n; v: increasing m.\n")
        f.write("# Absorption point of pixel (n, m) = (x0, y0, z0) + pitch * ((n - 4.5) u + (m - 4.5) v).\n")
        f.write("# ch order and pixel directions from the HA layout figure; the pixel directions are to be checked with data.\n")
        f.write("board,ch,x0,y0,z0,ux,uy,uz,vx,vy,vz,pitch\n")
        for r in rows:
            f.write(",".join([str(r[0]), str(r[1])] + [f"{v:.3f}" if isinstance(v, float) else str(v)
                                                     for v in r[2:]]) + "\n")
    print(f"wrote {args.output}: {len(rows)} blocks of {len(HA_LAYOUT)} HA")


if __name__ == "__main__":
    main()
