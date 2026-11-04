"""Run configurations of the EricaNext chain (config/*.json).

    import ericanext_config as ec
    cfg = ec.load("smile3_tpc_only")
    cfg["chain"]                     # module order
    cfg["parameters"]["CalcFADC"]    # {"base_window": 30.0, "threshold": 20.0, ...}

Keys starting with "_" are comments and are dropped by load(); in strings
"{data_root}" is replaced by data_root() and "{repo}" by the top of this repository.
"""
import json
import os
import sys
from pathlib import Path

CONFIG_DIR = Path(__file__).resolve().parent
REPO_DIR = CONFIG_DIR.parent

# The SMILE-3 DAQ data. Layout:
#   <data_root>/storage_g<n>/run_<YYYYMMDD>T<hhmmss>Z/   one run: tpc_enc_a<board>_<file>.raw, tpc_enc_c<board>_<file>.raw
# To use another directory, keep the same layout and set $SMILE3_DATA to the directory that holds storage_g0/ etc.
DATA_DIR = REPO_DIR / "mount_data"


def data_root():
    """{data_root}: directory that holds storage_g<n>/. $SMILE3_DATA, else DATA_DIR (mount_data/ in this repository).
    No other directory is searched: when no storage_g*/ is found there, a warning tells which directory was used."""
    root = Path(os.environ.get("SMILE3_DATA", DATA_DIR)).expanduser()
    if not any(root.glob("storage_g*")):
        src = "$SMILE3_DATA" if "SMILE3_DATA" in os.environ else "DATA_DIR (mount_data/)"
        print(f"ericanext_config: WARNING: no storage_g*/ in {root} (from {src}). "
              "SMILE3_DATA must be the directory that holds storage_g0/ etc.", file=sys.stderr)
    return root


def available():
    return sorted(p.stem for p in CONFIG_DIR.glob("*.json"))


def _resolve(obj, subs):
    if isinstance(obj, dict):
        return {k: _resolve(v, subs) for k, v in obj.items() if not k.startswith("_")}
    if isinstance(obj, list):
        return [_resolve(v, subs) for v in obj]
    if isinstance(obj, str):
        for key, value in subs.items():
            if key in obj:
                obj = obj.replace(key, str(value()))
        return obj
    return obj


def load(name):
    """Load config/<name>.json (or a path to a .json file)."""
    path = Path(name)
    if path.suffix != ".json":
        path = CONFIG_DIR / f"{name}.json"
    with open(path) as fp:
        raw = json.load(fp)
    return _resolve(raw, {"{data_root}": data_root, "{repo}": lambda: REPO_DIR})
