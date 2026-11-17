#!/usr/bin/env python3
"""Run the EricaNext chain with a configuration from config/stage1/*.json, config/stage2/*.json.

    python3 run_chain.py smile3_tpc_raw --data-dir <run> -o raw.root
    python3 run_chain.py smile3_tpc_only --root-input raw.root -o out.root
    python3 run_chain.py smile3_tpc_only -o out.root -n 10000 --data-dir ../mount_data/storage_g0/run_20260618T010203Z

SMILE-2+ data (modules *_2p from EricaNext_try):
    python3 run_chain.py smile2p_20190701_Per02 -o per02_out.root
    python3 run_chain.py smile2p_20190701_Per02 -o raw.root --raw -n 10000 --compression 505

--data-dir replaces ReadData.data_dir / PSA_ReadData.data_dir / ReadData_2p.data_dir (the run directory) of the config.
The SMILE-3 configs leave data_dir empty, so reading SMILE-3 raw data needs --data-dir (or --root-input for a raw ROOT).
--extra-data-dir adds another directory of the same run (SMILE-3 FM: the other storage node), e.g.
    python3 run_chain.py smile3_reconstruct -o run04.root --data-dir <NAS>/20261102/calltt/20261102_run04 \
        --extra-data-dir <NAS>/20261102/killua/20261102_run04
--raw runs only the reader and the raw data tree writer (ReadData -> RawTreeWriter, ReadData_2p -> RawTreeWriter_2p).
--first-entry N (with --root-input) starts at row N of the raw ROOT; with -n M it reads rows N to N+M-1, so a long
run can be split over several processes (scripts/calib/parallel_chain.py does that):
    python3 run_chain.py smile3_psa --root-input raw.root -o part01.root --first-entry 10000000 -n 10000000
--set Module.param=value overrides a parameter of the config (repeatable), e.g.
    python3 run_chain.py smile3_tpc_only -o out.root --set CalcFADC.threshold=30.0
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.join(HERE, "..")
# use this repository's build, not an older EricaPy in /usr/local/lib/python
sys.path.insert(0, os.path.join(REPO, "install", "lib", "python"))
sys.path.insert(0, os.path.join(REPO, "config"))
import anlnext
import EricaPy as th
import ericanext_config as ec
from anl_run import run_checked


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("config", help=f"config name or .json path ({', '.join(ec.available())})")
    ap.add_argument("-o", "--output", required=True, help="output ROOT file")
    ap.add_argument("-n", "--n-events", type=int, default=-1, help="number of events (-1 = all)")
    ap.add_argument("--data-dir", help="run directory (replaces ReadData.data_dir / PSA_ReadData.data_dir / ReadData_2p.data_dir of the config)")
    ap.add_argument("--extra-data-dir", action="append", default=[], metavar="DIR",
                    help="another directory of the same run (the other storage node), added to ReadData.extra_data_dirs "
                         "and PSA_ReadData.extra_data_dirs; repeatable")
    ap.add_argument("--root-input", help="RawTreeWriter ROOT input for SMILE-3 downstream chains")
    ap.add_argument("--first-entry", type=int, default=0,
                    help="with --root-input: first row of the raw ROOT to read (with -n, one part of a run)")
    ap.add_argument("--raw", action="store_true",
                    help="ReadData -> RawTreeWriter (SMILE-3) or ReadData_2p -> RawTreeWriter_2p (SMILE-2+) only")
    ap.add_argument("--compression", type=int, default=-1,
                    help="ROOT compression setting, 100*algorithm+level (505 = zstd 5, recommended with --raw); -1 = ROOT default")
    ap.add_argument("--set", action="append", default=[], metavar="MODULE.PARAM=VALUE",
                    help="override a module parameter (value parsed as JSON when possible), repeatable")
    args = ap.parse_args()

    cfg = ec.load(args.config)
    params = cfg["parameters"]
    readers = ("ReadData", "PSA_ReadData", "ReadData_2p")
    if args.data_dir:
        for m in readers:
            if m in cfg["chain"]:
                params.setdefault(m, {})["data_dir"] = os.path.abspath(args.data_dir)
    for d in args.extra_data_dir:
        for m in ("ReadData", "PSA_ReadData"):
            if m in cfg["chain"]:
                params.setdefault(m, {}).setdefault("extra_data_dirs", []).append(os.path.abspath(d))
    for text in args.set:
        path, value = text.split("=", 1)
        module, key = path.split(".", 1)
        try:
            value = json.loads(value)
        except json.JSONDecodeError:
            pass
        params.setdefault(module, {})[key] = value

    chain = cfg["chain"]
    if args.raw:
        chain = ["ReadData_2p", "RawTreeWriter_2p"] if "ReadData_2p" in chain else ["ReadData", "RawTreeWriter"]
    if args.root_input:
        if args.raw or args.data_dir or args.extra_data_dir or "ReadData_2p" in chain:
            ap.error("--root-input cannot be combined with raw data input options or SMILE-2+")
        if not any(m in chain for m in ("ReadData", "PSA_ReadData")):
            ap.error("this chain has no SMILE-3 reader")
        for m in ("ReadData", "PSA_ReadData"):
            if m in chain:
                params.setdefault(m, {})["root_file"] = os.path.abspath(args.root_input)
                if args.first_entry:
                    params[m]["root_first_entry"] = args.first_entry
    elif args.first_entry:
        ap.error("--first-entry needs --root-input")
    for m in readers:
        if m not in chain:
            continue
        if params.get(m, {}).get("root_file"):
            if os.path.realpath(params[m]["root_file"]) == os.path.realpath(args.output):
                ap.error("input ROOT and output must differ")
            continue
        data_dir = params.get(m, {}).get("data_dir") if m in chain else None
        if not data_dir and m != "ReadData_2p":
            ap.error(f"no run to read: give --data-dir <run directory> (or --root-input <raw ROOT>); "
                     f"{m}.data_dir of the config is empty")
        if data_dir and not os.path.isdir(data_dir):
            print(f"run_chain.py: no run directory {data_dir}", file=sys.stderr)
            sys.exit(1)
        for d in (params.get(m, {}).get("extra_data_dirs") or []) if m in chain else []:
            if not os.path.isdir(d):
                print(f"run_chain.py: no directory {d} (extra_data_dirs of {m})", file=sys.stderr)
                sys.exit(1)

    # write to <output>.part and rename it only when the chain succeeded, so that a failed run never leaves
    # a broken file that looks finished
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    part = args.output + ".part"
    if os.path.exists(part):
        os.remove(part)
    a = anlnext.AnalysisChain()
    a.chain(th.SaveFile)
    a.with_parameters({"filename": part, "compression": args.compression})
    for m in chain:
        a.chain(getattr(th, m))
        if params.get(m):
            a.with_parameters(params[m])
    ok = run_checked(a, args.n_events)
    if ok and os.path.exists(part):
        os.replace(part, args.output)
    else:
        if os.path.exists(part):
            os.remove(part)
        print(f"run_chain.py: failed, {args.output} not written", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
