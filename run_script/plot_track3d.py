#!/usr/bin/env python3

import sys
import uproot
import awkward as ak
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

ROOT_FILE = "output_3.root"
TREE_PATH = "TOT_Skewness/tot_skewness"
TARGET_ENTRY = 0  # コマンドライン引数で上書き可

def main():
    entry = int(sys.argv[1]) if len(sys.argv) > 1 else TARGET_ENTRY

    with uproot.open(ROOT_FILE) as f:
        tree = f[TREE_PATH]
        n = tree.num_entries
        if entry >= n:
            print(f"entry {entry} out of range (tree has {n} entries)")
            return

        cx = tree["cloud_x"].array(library="ak", entry_start=entry, entry_stop=entry+1)
        cy = tree["cloud_y"].array(library="ak", entry_start=entry, entry_stop=entry+1)
        cz = tree["cloud_z"].array(library="ak", entry_start=entry, entry_stop=entry+1)
        Pe  = tree["Pe"].array(library="np", entry_start=entry, entry_stop=entry+1)[0]
        Ve  = tree["Ve"].array(library="np", entry_start=entry, entry_stop=entry+1)[0]
        Len = tree["Len"].array(library="np", entry_start=entry, entry_stop=entry+1)[0]

    x = np.array(cx[0])
    y = np.array(cy[0])
    z = np.array(cz[0])
    nhits = len(x)
    print(f"entry={entry}  cloud_points={nhits}  Pe=({Pe[0]:.1f},{Pe[1]:.1f},{Pe[2]:.1f})  "
          f"Ve=({Ve[0]:.3f},{Ve[1]:.3f},{Ve[2]:.3f})  Len={Len:.1f} mm")

    if nhits == 0:
        print("cloud is empty for this entry")
        return

    fig = plt.figure(figsize=(8, 7))
    ax = fig.add_subplot(111, projection="3d")

    sc = ax.scatter(x, y, z, s=3, c=z, cmap="viridis", alpha=0.6, label="cloud")
    ax.scatter(*Pe, s=80, c="red", marker="*", label=f"Pe ({Pe[0]:.1f},{Pe[1]:.1f},{Pe[2]:.1f})")

    Ve_norm = Ve / np.linalg.norm(Ve)
    arrow_len = max(Len * 0.6, 10.)
    ax.quiver(Pe[0], Pe[1], Pe[2],
              Ve_norm[0]*arrow_len, Ve_norm[1]*arrow_len, Ve_norm[2]*arrow_len,
              color="orange", linewidth=2, arrow_length_ratio=0.15,
              label=f"Ve ({Ve_norm[0]:.2f},{Ve_norm[1]:.2f},{Ve_norm[2]:.2f})")

    plt.colorbar(sc, ax=ax, label="z [mm]", shrink=0.5)
    ax.set_xlabel("X [mm]")
    ax.set_ylabel("Y [mm]")
    ax.set_zlabel("Z [mm]")
    ax.set_title(f"3D coincidence cloud  entry={entry}  N={nhits}")
    ax.legend()
    plt.tight_layout()
    outfile = f"track3d_entry{entry}.png"
    plt.savefig(outfile, dpi=150)
    print(f"saved: {outfile}")
    plt.show()

if __name__ == "__main__":
    main()
