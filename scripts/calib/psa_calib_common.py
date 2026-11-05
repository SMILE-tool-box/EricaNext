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
