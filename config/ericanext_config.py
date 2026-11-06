"""Run configurations of the EricaNext chain (config/*.json).

    import ericanext_config as ec
    cfg = ec.load("smile3_tpc_only")
    cfg["chain"]                     # module order
    cfg["parameters"]["CalcFADC"]    # {"base_window": 30.0, "threshold": 20.0, ...}

Keys starting with "_" are comments and are dropped by load(); in strings
"{data_root}" is replaced by data_root() and "{repo}" by the top of this repository.
SMILE-2+ configurations (smile2p_*.json, tpc_caldata_*.json, modules *_2p from EricaNext_try):
"{data_top}" is replaced by data_top_2p(), "{tpc_caldata}" by tpc_caldata_2p() and "{sim_data}" by sim_data_2p().
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


# The SMILE-2+ data of EricaNext_try (on the lab NAS). Layout:
#   calibration_data/sm2+_cal_rawdata/<date>_<Period>/   raw data (uTPC_*.raw, HA*.raw) and drift_velocity.dat
#   calibration_data/param/<date>/                        calibration files (Calibration*.dat, MDF, Cal_HA, HAs.pos, ...)
#   TPC_caldata/<date>/<Period>_TPC/                      TPC-only calibration runs (Ba-133, 2017-12-19/20)
#   simulation_data/parameters/smile2p_sim_data/          simulation data
# To use a copy elsewhere, keep the same layout and set $SMILE2P_DATA to the directory that holds calibration_data/ etc.
DATA_DIR_2P = "/mnt/nas_eureka/onoda/EricaNext_try_data/SMILE2+"


def data_root_2p():
    """Top of the SMILE-2+ data: the directory with calibration_data/, TPC_caldata/ and simulation_data/ inside.
    $SMILE2P_DATA, else DATA_DIR_2P (the NAS). No other directory is searched."""
    root = Path(os.environ.get("SMILE2P_DATA", DATA_DIR_2P)).expanduser()
    if not any((root / d).is_dir() for d in ("calibration_data", "TPC_caldata", "simulation_data")):
        src = "$SMILE2P_DATA" if "SMILE2P_DATA" in os.environ else "DATA_DIR_2P (the NAS)"
        print(f"ericanext_config: WARNING: no calibration_data/, TPC_caldata/ or simulation_data/ in {root} (from {src}). "
              "SMILE2P_DATA must be the directory that holds them (= SMILE2+ on the NAS).", file=sys.stderr)
    return root


def data_top_2p():
    """{data_top}: directory that holds sm2+_cal_rawdata/ and param/."""
    return data_root_2p() / "calibration_data"


def tpc_caldata_2p():
    """{tpc_caldata}: directory of the SMILE-2+ TPC-only calibration runs (<date>/<Period>_TPC/)."""
    return data_root_2p() / "TPC_caldata"


def sim_data_2p():
    """{sim_data}: directory of the SMILE-2+ simulation data."""
    return data_root_2p() / "simulation_data" / "parameters" / "smile2p_sim_data"


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
    return _resolve(raw, {"{data_root}": data_root, "{repo}": lambda: REPO_DIR,
                          "{data_top}": data_top_2p, "{tpc_caldata}": tpc_caldata_2p, "{sim_data}": sim_data_2p})
