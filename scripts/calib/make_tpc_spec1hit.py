#!/usr/bin/env python3
"""TPC 1-hit spectra from the raw data tree (post-analysis; the first step of a TPC gain map).

    python3 run_script/run_chain.py smile3_raw -o raw.root --data-dir <TPC_cal run>
    /usr/bin/python3 scripts/calib/make_tpc_spec1hit.py -o spec1hit.root raw.root [raw2.root ...]

The charge of every FADC channel is computed from RawTreeWriter_sm3/raw (fadc_ch, fadc_wave) exactly as
CalcFADC does (baseline = mean of the first base_window samples, added sample by sample as in C++; charge =
sum of (wave - baseline) over the samples more than threshold above it). Cathode waveforms in the tree are
already inverted (1023 - ADC).

1 hit (SMILE_tool_box SPEC_1Hit): exactly one anode channel with charge and exactly one cathode channel with
charge in the same panel (the panel of a board = board / 3; FADC channel = 4 board + ch = strip / 32).
Board 1 hit: exactly one anode board and one cathode board of that panel with charge; the 4 channels of each
board are summed.

Output (ROOT file):
  Baseline_Anode, Baseline_Cathode      TH2D baseline x channel (channels with charge)
  Charge_Anode, Charge_Cathode          TH2D charge x channel (channels with charge)
  Anode_1Hit_A<aa>_C<cc>, Cathode_1Hit_A<aa>_C<cc>        TH1D per channel pair (aa, cc = 0-47)
  Anode_1Hit_board_A<i>_C<j>, Cathode_1Hit_board_A<i>_C<j>  TH1D per board pair (i, j = 0-11)
  Spectra: 2000 bins, 0-1e4 [ADU x sample] (as the former CalcFADC histograms).
and <output dir>/qa/<output name>_board.png (board 1-hit spectra per panel). Look at it before using the file.
"""
import argparse
import os
import sys

import ROOT

ROOT.gROOT.SetBatch(True)

CPP = r"""
#include <map>
#include <string>
#include <vector>
#include <TH1D.h>
#include <TH2D.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

struct Spec1Hit {
  int base_window; double threshold; int nclk;
  TH2D *base[2], *charge[2];
  std::map<std::string, TH1D*> h;
  long n_events = 0, n_1hit = 0, n_board = 0;

  Spec1Hit(int bw, double thr, int n) : base_window(bw), threshold(thr), nclk(n) {
    TH1::AddDirectory(kFALSE);   // the histograms belong to no file (the input files are closed)
    const char* side[2] = {"Anode", "Cathode"};
    for(int s=0; s<2; s++){
      base[s]   = new TH2D(Form("Baseline_%s", side[s]), Form("Baseline %s;[ADU];[ch]", side[s]), 256, -0.5, 255.5, 48, -0.5, 47.5);
      charge[s] = new TH2D(Form("Charge_%s", side[s]), Form("Charge %s;[ADU x sample];[ch]", side[s]), 2000, 0, 1e4, 48, -0.5, 47.5);
    }
  }
  TH1D* get(const std::string& name){
    auto it = h.find(name);
    if(it != h.end())  return it->second;
    TH1D* x = new TH1D(name.c_str(), (name + ";[ADU x sample];events").c_str(), 2000, 0, 1e4);
    h[name] = x;
    return x;
  }
  void fill(const std::vector<short>& chs, const std::vector<short>& wave){
    n_events++;
    double q[2][48] = {{0}};
    for(size_t i=0; i<chs.size(); i++){
      const short* w = &wave[i*nclk];
      double mean = 0, c = 0;
      for(int k=0; k<base_window; k++)  mean += w[k] / (double)base_window;   // as CalcFADC::IntegWave
      for(int k=base_window; k<nclk; k++)
        if(w[k] - mean > threshold)  c += w[k] - mean;
      int s = chs[i] >= 48, ch = chs[i] % 48;
      q[s][ch] = c;
      if(c > 0){ base[s]->Fill(mean, ch); charge[s]->Fill(c, ch); }
    }
    // channel 1 hit
    int na = 0, a = -1;
    for(int ch=0; ch<48; ch++)  if(q[0][ch] > 0){ na++; a = ch; }
    if(na == 1){
      int panel = (a / 4) / 3, nc = 0, c = -1;
      for(int ch=0; ch<48; ch++)  if((ch / 4) / 3 == panel && q[1][ch] > 0){ nc++; c = ch; }
      if(nc == 1){
        n_1hit++;
        get(Form("Anode_1Hit_A%02d_C%02d", a, c))->Fill(q[0][a]);
        get(Form("Cathode_1Hit_A%02d_C%02d", a, c))->Fill(q[1][c]);
      }
    }
    // board 1 hit
    int nba = 0, ba = -1;
    for(int b=0; b<12; b++){
      bool hit = false;
      for(int ch=0; ch<4; ch++)  hit |= q[0][4*b+ch] > 0;
      if(hit){ nba++; ba = b; }
    }
    if(nba == 1){
      int nbc = 0, bc = -1;
      for(int b=3*(ba/3); b<3*(ba/3)+3; b++){
        bool hit = false;
        for(int ch=0; ch<4; ch++)  hit |= q[1][4*b+ch] > 0;
        if(hit){ nbc++; bc = b; }
      }
      if(nbc == 1){
        n_board++;
        double sa = 0, sc = 0;
        for(int ch=0; ch<4; ch++){ sa += q[0][4*ba+ch]; sc += q[1][4*bc+ch]; }
        get(Form("Anode_1Hit_board_A%d_C%d", ba, bc))->Fill(sa);
        get(Form("Cathode_1Hit_board_A%d_C%d", ba, bc))->Fill(sc);
      }
    }
  }
  void run(TTree* t){
    TTreeReader r(t);
    TTreeReaderValue<std::vector<short>> chs(r, "fadc_ch"), wave(r, "fadc_wave");
    while(r.Next())  fill(*chs, *wave);
  }
  void write(){
    for(int s=0; s<2; s++){ base[s]->Write(); charge[s]->Write(); }
    for(auto& kv : h)  kv.second->Write();
  }
};
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="+", help="output ROOT files of smile3_raw (RawTreeWriter_sm3/raw)")
    ap.add_argument("-o", "--output", required=True, help="output ROOT file")
    ap.add_argument("--base-window", type=int, default=30, help="baseline samples (CalcFADC.base_window)")
    ap.add_argument("--threshold", type=float, default=20.0, help="charge threshold [ADU] (CalcFADC.threshold)")
    ap.add_argument("--overwrite", action="store_true")
    ap.add_argument("--no-qa", action="store_true", help="do not write the QA figure")
    args = ap.parse_args()

    if os.path.exists(args.output) and not args.overwrite:
        sys.exit(f"{args.output} exists (--overwrite to replace it)")
    ROOT.gInterpreter.Declare(CPP)

    spec = None
    for path in args.inputs:
        f = ROOT.TFile.Open(path)
        t = f.Get("RawTreeWriter_sm3/raw") if f else None
        if not t:
            sys.exit(f"{path}: no RawTreeWriter_sm3/raw")
        info = f.Get("RawTreeWriter_sm3/run_info")
        if info and info.GetEntries():
            info.GetEntry(0)
            if info.save_threshold > 0 and info.save_threshold >= args.threshold:
                print(f"WARNING: {path}: save_threshold {info.save_threshold} >= threshold {args.threshold}: "
                      "channels with a small charge are missing from the tree", file=sys.stderr)
        if t.GetEntries() == 0:
            continue
        t.GetEntry(0)
        nclk = len(t.fadc_wave) // max(len(t.fadc_ch), 1) if len(t.fadc_ch) else 512
        if spec is None:
            spec = ROOT.Spec1Hit(args.base_window, args.threshold, nclk)
        spec.run(t)
        print(f"{path}: {t.GetEntries()} events")
        f.Close()
    if spec is None:
        sys.exit("no events")

    out = ROOT.TFile(args.output, "recreate")
    spec.write()
    out.Close()
    print(f"wrote {args.output}: {spec.n_events} events, channel 1 hit {spec.n_1hit}, board 1 hit {spec.n_board}")

    if not args.no_qa:
        write_qa(args.output)


QA_REBIN = 10


def write_qa(path):
    import numpy as np
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    f = ROOT.TFile.Open(path)
    fig, axs = plt.subplots(2, 2, figsize=(14, 8), sharex=True)
    for k in f.GetListOfKeys():
        name = k.GetName()
        if not name.startswith("Anode_1Hit_board_"):
            continue
        h = k.ReadObj()
        y = np.array([h.GetBinContent(i) for i in range(1, h.GetNbinsX() + 1)])
        x = np.array([h.GetBinCenter(i) for i in range(1, h.GetNbinsX() + 1)])
        y, x = y.reshape(-1, QA_REBIN).sum(1), x.reshape(-1, QA_REBIN).mean(1)   # 50 ADU x sample per bin
        ba = int(name.split("_A")[1].split("_")[0])
        ax = axs.flat[ba // 3]
        ax.step(x, y, where="mid", lw=0.8, label=f"{name[17:]} ({int(y.sum())})")
    for p, ax in enumerate(axs.flat):
        ax.set_title(f"uPIC{p}: board 1 hit, anode charge"); ax.set_yscale("log"); ax.set_xlim(0, 4000)
        ax.set_xlabel("anode charge [ADU x sample]"); ax.set_ylabel(f"events / {5 * QA_REBIN} ADU x sample")
        if ax.get_legend_handles_labels()[0]:
            ax.legend(fontsize=7, ncol=3)
    fig.tight_layout()
    qa = os.path.join(os.path.dirname(os.path.abspath(path)), "qa")
    os.makedirs(qa, exist_ok=True)
    png = os.path.join(qa, os.path.splitext(os.path.basename(path))[0] + "_board.png")
    fig.savefig(png, dpi=70)
    print("wrote", png)


if __name__ == "__main__":
    main()
