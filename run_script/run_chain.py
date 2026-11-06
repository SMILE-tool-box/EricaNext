#!/usr/bin/env python3
"""Run the EricaNext chain with a configuration from config/*.json.

    python3 run_chain.py smile3_tpc_only -o out.root
    python3 run_chain.py smile3_tpc_only -o out.root -n 10000 --data-dir ../mount_data/storage_g0/run_20260618T010203Z

SMILE-2+ data (modules *_2p from EricaNext_try):
    python3 run_chain.py smile2p_20190701_Per02 -o per02_out.root
    python3 run_chain.py smile2p_20190701_Per02 -o raw.root --raw -n 10000 --compression 505

--data-dir replaces ReadData.data_dir / PSA_ReadData.data_dir / ReadData_2p.data_dir (the run directory) of the config.
--raw runs only the reader and the raw data tree writer (ReadData -> RawTreeWriter_sm3, ReadData_2p -> RawTreeWriter_2p).
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
    ap.add_argument("--raw", action="store_true",
                    help="ReadData -> RawTreeWriter_sm3 (SMILE-3) or ReadData_2p -> RawTreeWriter_2p (SMILE-2+) only")
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
        chain = ["ReadData_2p", "RawTreeWriter_2p"] if "ReadData_2p" in chain else ["ReadData", "RawTreeWriter_sm3"]
    for m in readers:
        data_dir = params.get(m, {}).get("data_dir") if m in chain else None
        if data_dir and not os.path.isdir(data_dir):
            print(f"run_chain.py: no run directory {data_dir}", file=sys.stderr)
            sys.exit(1)

    # write to <output>.part and rename it only when the chain succeeded, so that a failed run never leaves
    # a broken file that looks finished
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
