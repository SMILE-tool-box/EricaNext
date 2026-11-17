"""Common parts of the PSA calibration scripts (make_psa_temp_gain.py, make_psa_energy.py).

The inputs are outputs of run_script/run_chain.py smile3_psa (tree PSA_TreeWriter/psa) of calibration runs.
"""
import datetime
import os

import numpy as np

# HA_cal etc. share the trigger between the boards: a board also records frames for the triggers of the
# other boards, and pulses that are not at the trigger time come in. Their integration window is cut at
# the end of the waveform and hg comes out small, so only hits with peak_id at the trigger time are used.
PEAK_ID_MIN = 25


def read_tree(files, columns, selection=""):
    """columns of PSA_TreeWriter/psa of all files as numpy arrays (short -> int)"""
    import ROOT
    chain = ROOT.TChain("PSA_TreeWriter/psa")
    for f in files:
        chain.Add(f)
    df = ROOT.RDataFrame(chain)
    if selection:
        df = df.Filter(selection)
    d = df.AsNumpy(columns)
    return {k: (v.astype(int) if v.dtype == object else v) for k, v in d.items()}


N_BOARDS, N_PEAK_ID = 64, 4096   # board numbers and peak_id values covered by the RDataFrame histograms
_N_DECLARED = 0


def rdataframe(files, selection=""):
    """RDataFrame of PSA_TreeWriter/psa of all files (the chain is kept alive by the returned frame)"""
    import ROOT
    ROOT.gROOT.SetBatch(True)
    chain = ROOT.TChain("PSA_TreeWriter/psa")
    for f in files:
        chain.Add(f)
    df = ROOT.RDataFrame(chain)
    df._chain = chain
    return df.Filter(selection) if selection else df


def declare(code_with_name):
    """gInterpreter.Declare a C++ function; code_with_name(name) -> code. Returns the (unique) name."""
    import ROOT
    global _N_DECLARED
    name = f"psa_calib_fn{_N_DECLARED}"
    _N_DECLARED += 1
    ROOT.gInterpreter.Declare(code_with_name(name))
    return name


def hist_array(hist, nx, ny):
    """contents of a TH2D as int64 [x][y] including the under/overflow bins of both axes"""
    a = np.frombuffer(hist.GetArray(), dtype=np.float64, count=(nx + 2) * (ny + 2))
    return a.reshape(ny + 2, nx + 2).T.round().astype(np.int64)


def board_peak_ranges(df, boards=None, window=None):
    """{board: (lo, hi)} trigger-time peak_id range (on_time_range) of every board with hits in df, without
    reading the hits into memory (one pass)"""
    h = df.Histo2D(("pid", "", N_BOARDS, 0, N_BOARDS, N_PEAK_ID, 0, N_PEAK_ID), "board", "peak_id").GetValue()
    counts = hist_array(h, N_BOARDS, N_PEAK_ID)
    if counts.sum() != counts[1:-1, 1:-1].sum():
        raise RuntimeError(f"hits with board outside 0-{N_BOARDS - 1} or peak_id outside 0-{N_PEAK_ID - 1}")
    counts = counts[1:-1, 1:-1]
    r = {}
    for b in range(N_BOARDS):
        c = counts[b]
        if not c.any() or (boards is not None and b not in boards):
            continue
        last = max(int(np.nonzero(c)[0][-1]) + 1, 50)   # np.bincount(peak_id, minlength=50)
        r[b] = on_time_range(np.repeat(np.arange(last), c[:last]), window)
    return r


def peak_range_filter(ranges):
    """name of a C++ function bool f(board, peak_id): peak_id inside the range of its board"""
    lo = [ranges.get(b, (0, -1))[0] for b in range(N_BOARDS)]
    hi = [ranges.get(b, (0, -1))[1] for b in range(N_BOARDS)]
    return declare(lambda name: f"""
    bool {name}(int b, int p) {{
        static const int lo[] = {{{", ".join(map(str, lo))}}}, hi[] = {{{", ".join(map(str, hi))}}};
        return b >= 0 && b < {N_BOARDS} && p >= lo[b] && p <= hi[b];
    }}""")


def on_time_range(peak_id, window=None):
    """peak_id range (both ends included) of the pulses at the trigger time (temp_spectra.py on_time_range).
    window given: most frequent peak_id +- window. Else from the most frequent peak_id outward while the
    count is above twice the off-time background (median of 20-8 samples before / 5-17 samples after)."""
    counts = np.bincount(peak_id, minlength=50)
    mode = int(counts.argmax())
    if window is not None:
        return mode - window, mode + window
    before = np.median(counts[max(0, mode - 20):max(1, mode - 7)])
    after = np.median(counts[min(mode + 5, 48):min(mode + 18, 49)])
    low, high = mode, mode
    while low - 1 > PEAK_ID_MIN and counts[low - 1] > 2 * before:
        low -= 1
    while high + 1 < len(counts) - 1 and counts[high + 1] > 2 * after:
        high += 1
    return low, high


def photopeak(counts, centers, peak_min, min_height=0.3):
    """photopeak position (temp_spectra.py photopeak): of the maxima of the 5-bin smoothed spectrum above
    peak_min that are at least min_height of the highest, the one with the highest energy (the photopeak
    can be lower than the Compton part). NaN when there is nothing above peak_min."""
    smoothed = np.convolve(counts, np.ones(5) / 5, mode="same")
    above = centers >= peak_min
    if not np.any(smoothed[above] > 0):
        return np.nan
    local_max = np.zeros(len(smoothed), dtype=bool)
    local_max[1:-1] = (smoothed[1:-1] >= smoothed[:-2]) & (smoothed[1:-1] > smoothed[2:])
    cand = np.flatnonzero(local_max & above & (smoothed >= min_height * smoothed[above].max()))
    if len(cand) == 0:
        return centers[above][np.argmax(smoothed[above])]
    return centers[cand[-1]]


def header(script, files, lines):
    """comment lines (# ...) written at the top of a calibration file"""
    now = datetime.datetime.now().isoformat(timespec="seconds")
    out = [f"# made by scripts/calib/{script}, {now}"]
    out += [f"# input: {os.path.abspath(f)}" for f in files]
    out += [f"# {l}" for l in lines]
    return "\n".join(out) + "\n"
