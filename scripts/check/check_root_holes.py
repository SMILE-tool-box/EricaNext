#!/usr/bin/env python3
"""Find holes (runs of zero bytes) in ROOT files and the tree rows they make unreadable.

    /usr/bin/python3 check_root_holes.py <file.root> [<file.root> ...] [--min-run 256] [--chunk-mb 64]

Some raw ROOT files written to the NAS (CIFS) have pieces of 1-2 kB filled with zeros, ending at a 4 kB page
boundary (e.g. HA_raw_20261115_run06.root, 2026-11-17): the pages were lost when the file was written. ROOT
cannot read the baskets there ("basket: has fNevBuf=0 ... trying to repair", GetEntry returns -1), and
ReadData / PSA_ReadData skip those rows with a warning (root_skip_unreadable). This script finds them before use.

- Reads every byte of the file once (on the NAS about 40 MB/s, so a 100 GB file takes ~45 min).
- A hole is a run of at least --min-run zero bytes (compressed baskets practically never have such runs).
- Each hole is matched to the baskets it overlaps (all branches of all trees): tree, branch, rows.
- Exit status: 0 no hole, 1 holes found, 2 error.
"""
import argparse
import sys
import time
from pathlib import Path

import numpy as np


def zero_runs(path, min_run, chunk):
    """[(start, end)] of zero-byte runs of at least min_run bytes"""
    runs = []
    size = Path(path).stat().st_size
    t0 = time.time()
    run_start = None   # start of a zero run continuing from the previous chunk
    pos = 0
    last_report = t0
    with open(path, "rb", buffering=0) as f:
        while True:
            buf = f.read(chunk)
            if not buf:
                break
            a = np.frombuffer(buf, dtype=np.uint8)
            z = np.concatenate(([0], (a == 0).view(np.int8), [0]))
            d = np.diff(z)
            starts = np.flatnonzero(d == 1)
            ends = np.flatnonzero(d == -1)
            keep = (ends - starts >= min_run) | (starts == 0) | (ends == len(a))
            carry = None
            for s, e in zip(starts[keep].tolist(), ends[keep].tolist()):
                s_abs = run_start if (s == 0 and run_start is not None) else pos + s
                if e == len(a):          # continues into the next chunk
                    carry = s_abs
                elif pos + e - s_abs >= min_run:
                    runs.append((s_abs, pos + e))
            run_start = carry
            pos += len(a)
            now = time.time()
            if now - last_report > 30:
                print(f"  {pos / 1e9:.1f} / {size / 1e9:.1f} GB, {pos / 1e6 / (now - t0):.0f} MB/s, "
                      f"{len(runs)} holes so far", flush=True)
                last_report = now
    if run_start is not None and pos - run_start >= min_run:
        runs.append((run_start, pos))
    return runs


def baskets(path):
    """sorted arrays (seek, nbytes, label index, first row, last row) of all baskets on disk, and the labels"""
    import ROOT
    ROOT.gErrorIgnoreLevel = ROOT.kError
    f = ROOT.TFile.Open(str(path))
    if not f or f.IsZombie():
        raise OSError(f"cannot open {path}")
    labels, rows = [], []

    def walk_dir(d, prefix):
        names = set()
        for key in d.GetListOfKeys():
            name = f"{prefix}{key.GetName()}"
            if name in names:   # older cycle of a tree (AutoSave): its baskets are those of the newest cycle
                continue
            names.add(name)
            cl = ROOT.TClass.GetClass(key.GetClassName())
            if not cl or not (cl.InheritsFrom("TDirectory") or cl.InheritsFrom("TTree")):
                continue
            obj = key.ReadObj()
            if not obj:
                print(f"  WARNING: cannot read {name} ({key.GetClassName()}); its baskets are not matched", flush=True)
            elif cl.InheritsFrom("TDirectory"):
                walk_dir(obj, name + "/")
            else:
                seen = set()
                for leaf in obj.GetListOfLeaves():
                    br = leaf.GetBranch()
                    if br.GetName() not in seen:   # a branch can have several leaves
                        seen.add(br.GetName())
                        add_branch(br, name)

    def add_branch(br, tree):
        n = br.GetWriteBasket()
        if n <= 0:
            return
        k = len(labels)
        labels.append((tree, br.GetName(), int(br.GetEntries())))
        seeks = np.array([br.GetBasketSeek(i) for i in range(n)], dtype=np.int64)
        nbytes = np.array([br.GetBasketBytes()[i] for i in range(n)], dtype=np.int64)
        entry = np.array([br.GetBasketEntry()[i] for i in range(n + 1)], dtype=np.int64)
        entry[n] = max(entry[n], int(br.GetEntries()))
        ok = seeks > 0
        rows.append(np.column_stack([seeks[ok], nbytes[ok], np.full(ok.sum(), k), entry[:-1][ok], entry[1:][ok] - 1]))

    walk_dir(f, "")
    f.Close()
    if not rows:
        return np.zeros((0, 5), dtype=np.int64), labels
    b = np.concatenate(rows)
    return b[np.argsort(b[:, 0])], labels


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+")
    ap.add_argument("--min-run", type=int, default=256, help="minimum length of a zero run [bytes]")
    ap.add_argument("--chunk-mb", type=int, default=64, help="read size [MB]")
    a = ap.parse_args()
    status = 0
    for path in a.files:
        print(f"== {path}", flush=True)
        try:
            b, labels = baskets(path)
            print(f"  {len(b)} baskets in {len(labels)} branches", flush=True)
            runs = zero_runs(path, a.min_run, a.chunk_mb << 20)
        except Exception as e:  # noqa: BLE001
            print(f"  ERROR: {e}", flush=True)
            status = max(status, 2)
            continue
        if not runs:
            print("  no hole", flush=True)
            continue
        status = max(status, 1)
        bad = {}   # tree -> set of (first, last) row ranges
        for s, e in runs:
            aligned = " (ends at a 4 kB boundary)" if e % 4096 == 0 else ""
            print(f"  hole at byte {s} - {e} ({e - s} bytes){aligned}", flush=True)
            # baskets overlapping [s, e)
            i = np.searchsorted(b[:, 0], e, side="left")
            hit = False
            for j in range(i - 1, -1, -1):
                seek, nb, k, first, last = (int(x) for x in b[j])
                if seek + nb <= s:
                    break
                tree, branch, _ = labels[k]
                print(f"    {tree} branch {branch}: rows {first} - {last}", flush=True)
                bad.setdefault(tree, set()).add((first, last))
                hit = True
            if not hit:
                print("    outside the baskets (header, keys or free space)", flush=True)
        for tree, ranges in bad.items():
            mask_rows = 0
            cur = None
            for first, last in sorted(ranges):
                if cur and first <= cur[1] + 1:
                    cur[1] = max(cur[1], last)
                else:
                    if cur:
                        mask_rows += cur[1] - cur[0] + 1
                    cur = [first, last]
            mask_rows += cur[1] - cur[0] + 1
            n = max(l[2] for l in labels if l[0] == tree)
            print(f"  {tree}: {mask_rows} of {n} rows unreadable ({100 * mask_rows / max(n, 1):.4f} %)", flush=True)
    sys.exit(status)


if __name__ == "__main__":
    main()
