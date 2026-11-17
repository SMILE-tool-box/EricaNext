"""Run run_chain.py on raw ROOT files in parallel, each run split into parts of rows (run_chain.py --first-entry, -n).

    from parallel_chain import run_parallel
    outs = run_parallel([(raw, CACHE / "psa_cal_run06.root"), ...], "smile3_psa", ["--set", "..."],
                        chunk_events=10_000_000, max_jobs=8)
    # outs[i]: list of output ROOT files of the i-th input (one file per part), in row order

- The parts of one run are <out stem>.partNNN.root (NNN from 000), each with a log <part>.log and the conditions
  <part>.inputs.json (command, raw file size and modification time). A part whose output exists is reused only
  when its conditions are the same; otherwise this stops (give another output directory or delete the files).
- An output <out> that already exists as one file (from a run without parts) is used as it is.
- The rows of a part are independent of the other parts (each row is one event of the raw ROOT), so the parts
  together are the same as one run over the whole file.
"""
import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RUN_CHAIN = REPO / "run_script" / "run_chain.py"
RAW_TREES = ("PSA_RawTreeWriter/raw", "TPC_RawTreeWriter/raw", "RawTreeWriter_sm3/raw")


def n_rows(raw):
    """number of rows (events) of the raw tree of a RawTreeWriter ROOT"""
    import ROOT
    f = ROOT.TFile.Open(str(raw))
    if not f or f.IsZombie():
        raise OSError(f"cannot open {raw}")
    try:
        for name in RAW_TREES:
            t = f.Get(name)
            if t:
                return int(t.GetEntries())
        raise ValueError(f"{raw}: no raw tree ({', '.join(RAW_TREES)})")
    finally:
        f.Close()


def _plan(raw, out, config, args, chunk_events, python):
    raw, out = Path(raw), Path(out)
    if out.exists():
        return [], [out]
    n = n_rows(raw)
    st = raw.stat()
    jobs, outs = [], []
    for k, first in enumerate(range(0, n, chunk_events)):
        part = out.with_name(f"{out.stem}.part{k:03d}.root")
        count = min(chunk_events, n - first)
        cmd = [str(python), str(RUN_CHAIN), config, "--root-input", str(raw), "-o", str(part),
               "--first-entry", str(first), "-n", str(count), *[str(a) for a in args]]
        stamp = {"command": cmd, "raw_size": st.st_size, "raw_mtime_ns": st.st_mtime_ns}
        stamp_file = part.with_name(part.name[:-len(".root")] + ".inputs.json")
        if part.exists():
            if not stamp_file.exists() or json.loads(stamp_file.read_text()) != stamp:
                raise RuntimeError(f"{part}: made with other conditions; use another output directory or delete it")
        else:
            jobs.append((cmd, part, stamp_file, stamp))
        outs.append(part)
    return jobs, outs


def _run(job):
    cmd, part, stamp_file, stamp = job
    log = part.with_name(part.name[:-len(".root")] + ".log")
    with log.open("w") as f:
        r = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT)
    if r.returncode != 0 or not part.exists():
        return f"{part.name}: failed (see {log})"
    stamp_file.write_text(json.dumps(stamp, ensure_ascii=False, indent=2))
    return None


def run_parallel(pairs, config, args=(), chunk_events=10_000_000, max_jobs=8, python=sys.executable):
    """pairs: [(raw ROOT, output ROOT), ...]. Returns the list of the output parts of each pair."""
    if chunk_events <= 0 or max_jobs <= 0:
        raise ValueError("chunk_events and max_jobs must be positive")
    jobs, outs = [], []
    for raw, out in pairs:
        j, o = _plan(raw, out, config, args, chunk_events, python)
        jobs += j
        outs.append(o)
    print(f"{len(jobs)} parts to run, {sum(map(len, outs)) - len(jobs)} already done, {max_jobs} at a time", flush=True)
    errors = []
    with ThreadPoolExecutor(max_jobs) as ex:
        futures = {ex.submit(_run, j): j[1].name for j in jobs}
        for i, fut in enumerate(as_completed(futures), 1):
            err = fut.result()
            print(f"  {i}/{len(jobs)} {futures[fut]} {'done' if err is None else 'FAILED'}", flush=True)
            if err:
                errors.append(err)
    if errors:
        raise RuntimeError("some parts failed:\n" + "\n".join(errors))
    return outs
