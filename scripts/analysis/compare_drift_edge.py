"""Compare the electron-track maps (strip x clock) of SMILE-2+ and SMILE-3 FM near the drift-cage wall.

Both maps are filled the same way: every strip hit of the first N events of the raw tree
(hit_<side>_strip vs hit_<side>_clock), no event selection. Each map is divided by the mean of a
reference box well inside the detector, so the colour scale (0-1.5) means the same in both.
The lower pads show the hits per strip in three drift-time bands, normalised the same way:
a field distortion near the wall shows up as fewer hits near the wall at long drift times.

Usage (/usr/bin/python3, needs PyROOT):
    compare_drift_edge.py <SMILE-2+ raw ROOT> <SMILE-3 raw ROOT> -o compare.png [-n 500000] [--n-smile3 N] [--side a] [--upic 2]
SMILE-2+ raw ROOT: run_chain.py smile2p_20190701_Per02 --raw.  SMILE-3 raw ROOT: RawTreeWriter output.
"""
import argparse
import sys
from pathlib import Path
import ROOT

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tpc_raw_io import raw_tree_path

NSTRIP = 384                 # strips per μPIC (SMILE-2+ has one μPIC, SMILE-3 has four) [strip]
NCLK = 1024                  # clock range of the encoder [clock]
CLK_REBIN = 8                # clocks per bin in the maps [clock]
BANDS = [(150, 400), (400, 650), (650, 900)]  # drift-time bands for the profiles [clock]
REF_CLK = (150, 900)         # clock range of the reference box [clock]
REF_STRIP_2P = (150, 230)    # SMILE-2+ reference strips (well inside) [strip]
REF_STRIP_3 = (250, 330)     # SMILE-3 reference strips (good strips well inside the μPIC) [strip]

ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("smile2p")
ap.add_argument("smile3")
ap.add_argument("-o", "--output", required=True, help="output image (png, pdf, ...)")
ap.add_argument("-n", "--n-events", type=int, default=500000, help="first N events of each file (-1 = all)")
ap.add_argument("--n-smile3", type=int, default=None,
                help="first N events of the SMILE-3 file (-1 = all; default: same as -n)")
ap.add_argument("--side", choices=["a", "c"], default="a", help="a = anode, c = cathode")
ap.add_argument("--upic", type=int, default=2,
                help="SMILE-3 μPIC (0-3); the outer wall side is anode strip 383 / cathode strip 0 of each μPIC")
ap.add_argument("--counts", action="store_true", help="draw the accumulated hit counts as they are (no normalisation)")
args = ap.parse_args()

ROOT.gROOT.SetBatch(True)
ROOT.gStyle.SetOptStat(0)
ROOT.gStyle.SetPalette(ROOT.kRainBow)
ROOT.gStyle.SetNumberContours(100)


def fill_map(path, tree, name, strip_offset, n_events):
    """TH2D of all strip hits (strip - strip_offset, clock) of the first N events, and the number of events."""
    df = ROOT.RDataFrame(tree, path)
    if n_events > 0:
        df = df.Range(n_events)
    s = args.side
    df = df.Define("strip", f"ROOT::VecOps::RVec<double>(hit_{s}_strip) - {strip_offset}") \
           .Define("clock", f"ROOT::VecOps::RVec<double>(hit_{s}_clock)")
    h = df.Histo2D((name, "", NSTRIP, -0.5, NSTRIP - 0.5, NCLK // CLK_REBIN, -0.5, NCLK - 0.5), "strip", "clock")
    n = df.Count()
    return h.GetValue(), n.GetValue()


def normalise(h, ref_strip):
    """Divide by the mean content of the reference box (strips ref_strip x clocks REF_CLK)."""
    x1, x2 = h.GetXaxis().FindBin(ref_strip[0]), h.GetXaxis().FindBin(ref_strip[1] - 1)
    y1, y2 = h.GetYaxis().FindBin(REF_CLK[0]), h.GetYaxis().FindBin(REF_CLK[1] - 1)
    mean = h.Integral(x1, x2, y1, y2) / ((x2 - x1 + 1) * (y2 - y1 + 1))
    h.Scale(1.0 / mean)


def band_profiles(h, ref_strip):
    """Hits per strip in each drift-time band, divided by the mean of the reference strips."""
    out = []
    for lo, hi in BANDS:
        p = h.ProjectionX(f"{h.GetName()}_{lo}", h.GetYaxis().FindBin(lo), h.GetYaxis().FindBin(hi - 1))
        x1, x2 = p.FindBin(ref_strip[0]), p.FindBin(ref_strip[1] - 1)
        p.Scale((x2 - x1 + 1) / p.Integral(x1, x2))
        out.append(p)
    return out


side_name = "Anode" if args.side == "a" else "Cathode"
# Outer wall side of a SMILE-3 μPIC (local strip), from TOT_Skewness::Get_TPC_XYZ:
# anode = ±(strip + 0.5) pitch, cathode = ±(strip - 383.5) pitch from the centre of the four μPICs.
wall_strip = NSTRIP - 1 if args.side == "a" else 0
h2, n2 = fill_map(args.smile2p, "RawTreeWriter_2p/raw", "smile2p", 0, args.n_events)
h3, n3 = fill_map(args.smile3, raw_tree_path(args.smile3), "smile3", NSTRIP * args.upic,
                 args.n_events if args.n_smile3 is None else args.n_smile3)
if not args.counts:
    normalise(h2, REF_STRIP_2P)
    normalise(h3, REF_STRIP_3)
zlabel = "hits" if args.counts else "hits / reference"
h2.SetTitle(f"SMILE-2+ {Path(args.smile2p).stem} ({n2} events);{side_name} strip;drift time (clock);{zlabel}")
h3.SetTitle(f"SMILE-3 FM {Path(args.smile3).stem}, #muPIC{args.upic} ({n3} events);"
            f"{side_name} strip ({wall_strip} = outer wall side);drift time (clock);{zlabel}")

c = ROOT.TCanvas("c", "", 1600, 1250)
c.Divide(2, 2)
keep = []
colors = [ROOT.kBlue, ROOT.kGreen + 2, ROOT.kRed]
for k, (h, ref) in enumerate([(h2, REF_STRIP_2P), (h3, REF_STRIP_3)]):
    pad = c.cd(k + 1)
    pad.SetRightMargin(0.15)
    h.GetYaxis().SetRangeUser(0, 1000)
    h.SetMinimum(0)
    if not args.counts:
        h.SetMaximum(1.5)
    h.Draw("colz")

    pad = c.cd(k + 3)
    pad.SetGrid()
    leg = ROOT.TLegend(0.40, 0.15, 0.75, 0.35)
    for i, (p, col, (lo, hi)) in enumerate(zip(band_profiles(h, ref), colors, BANDS)):
        p.SetTitle(f"hits per strip by drift time;{side_name} strip;hits / reference")
        p.SetLineColor(col)
        p.SetLineWidth(2)
        p.SetMinimum(0)
        p.SetMaximum(1.6)
        p.Draw("hist" if i == 0 else "hist same")
        leg.AddEntry(p, f"clock {lo}-{hi}", "l")
        keep.append(p)  # PyROOT deletes objects without a Python reference
    leg.Draw()
    keep.append(leg)
c.SaveAs(args.output)
