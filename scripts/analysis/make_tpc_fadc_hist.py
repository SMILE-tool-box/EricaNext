#!/usr/bin/env python3
"""TPC_RawTreeWriterのFADC波形を全ボード・全chで積算し、1つのROOTへ保存する。"""
import argparse
import json
import os
from pathlib import Path
import sys

import ROOT
from tpc_raw_io import raw_tree_path, run_info_path
ROOT.gROOT.SetBatch(True)

CPP = r"""
#include <vector>
#include <stdexcept>
#include <string>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>
#include <TH2D.h>
namespace EricaFADCAccum {
class Accumulator {
public:
  int nclk;
  long long events = 0;
  long long waves[96] = {};
  TH2D* hist[96];
  Accumulator(int n) : nclk(n) {
    for(int g=0;g<96;g++) {
      std::string name = "FADC_" + std::string(g<48 ? "Anode" : "Cathode")
                         + std::to_string((g%48)/4) + "_Ch" + std::to_string(g%4);
      hist[g] = new TH2D(name.c_str(),(name+";FADC sample;ADC [ADU]").c_str(),
                        nclk,-0.5,nclk-0.5,1024,-0.5,1023.5);
      hist[g]->SetDirectory(nullptr);
    }
  }
  ~Accumulator(){for(auto* h:hist) delete h;}
  TH2D* get(int g){return hist[g];}
  long long count(int g){return waves[g];}
  void run(TTree* tree, long long limit) {
    TTreeReader reader(tree);
    TTreeReaderValue<std::vector<short>> ch(reader,"fadc_ch"), wave(reader,"fadc_wave");
    long long n = 0;
    while((limit<0 || n<limit) && reader.Next()) {
      if(wave->size()!=ch->size()*nclk) throw std::runtime_error("fadc_wave length mismatch");
      bool seen[96] = {};
      for(size_t i=0;i<ch->size();i++) {
        const int g=(*ch)[i];
        if(g<0 || g>=96 || seen[g]) throw std::runtime_error("invalid/duplicate fadc_ch");
        seen[g]=true;
        double* bins=hist[g]->GetArray();
        for(int s=0;s<nclk;s++) {
          const int adc=(*wave)[i*nclk+s];
          if(adc<0 || adc>1023) throw std::runtime_error("ADC outside 0..1023");
          // TH2D storage includes under/overflow bins on both axes.
          bins[(adc+1)*(nclk+2)+(s+1)] += 1.0;
        }
        waves[g]++;
      }
      events++;n++;
      if(n%100000==0) {std::cout << "Accumulated events: " << events << std::endl;}
    }
    if(reader.GetEntryStatus()!=TTreeReader::kEntryBeyondEnd && (limit<0 || n<limit))
      throw std::runtime_error("TTreeReader ended before the requested entries");
  }
  void finish(){
    for(int g=0;g<96;g++) {
      hist[g]->ResetStats();
      hist[g]->SetEntries(double(waves[g])*nclk);
    }
  }
};
}
"""


def validate(path):
    f=ROOT.TFile.Open(str(path))
    if not f or f.IsZombie(): raise ValueError(f'{path}: ROOTを開けません')
    try:
        t=f.Get(raw_tree_path(path));info=f.Get(run_info_path(path))
        if not t or not info or info.GetEntries()!=1: raise ValueError(f'{path}: raw/run_infoがありません')
        for name in ['fadc_ch','fadc_wave']:
            b=t.GetBranch(name)
            if not b or b.GetClassName().replace('std::','').replace(' ','')!='vector<short>':
                raise ValueError(f'{path}: {name}にvector<short>が必要です（save_fadc=true）')
        info.GetEntry(0);nclk=int(info.n_fadc_clk)
        if nclk<=0: raise ValueError(f'{path}: n_fadc_clkが不正です')
        return nclk,int(t.GetEntries()),float(info.save_threshold)
    finally:
        f.Close()


def write_pngs(output, directory):
    directory.mkdir(parents=True,exist_ok=True)
    f=ROOT.TFile.Open(str(output))
    ROOT.gStyle.SetOptStat(0)
    try:
        for side in ['Anode','Cathode']:
            for board in range(12):
                canvas=ROOT.TCanvas(f'c_{side}{board}','',1400,900);canvas.Divide(2,2)
                for ch in range(4):
                    h=f.Get(f'FADC_Accum/FADC_{side}{board}_Ch{ch}')
                    pad=canvas.cd(ch+1);pad.SetRightMargin(0.15);pad.SetLogz()
                    h.SetMinimum(1);h.Draw('COLZ')
                canvas.SaveAs(str(directory/f'FADC_{side}{board}.png'));canvas.Close()
    finally:
        f.Close()


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('inputs',nargs='+',help='TPC_RawTreeWriter/rawを含むROOT（同じ測定条件）')
    ap.add_argument('-o','--output',required=True,help='積算ヒストグラムをまとめる出力ROOT')
    ap.add_argument('-n','--max-events',type=int,default=-1,help='全入力を通じた先頭イベント数。-1は全件 [events]')
    ap.add_argument('--png-dir',type=Path,help='ボードごとの4ch確認PNGの保存先（省略時は描画しない）')
    ap.add_argument('--overwrite',action='store_true',help='既存の出力ROOTを置換する')
    args=ap.parse_args()
    if args.max_events==0 or args.max_events < -1:ap.error('--max-eventsは正の整数か-1を指定してください')
    paths=[Path(p).expanduser().resolve() for p in args.inputs]
    output=Path(args.output).expanduser().resolve()
    if len(set(paths))!=len(paths):ap.error('同じ入力ROOTが重複しています')
    if output in paths:ap.error('入力ROOTを出力で上書きできません')
    if output.exists() and not args.overwrite:ap.error(f'{output}は既存です（--overwriteで置換）')
    info=[validate(p) for p in paths]
    if len({x[0] for x in info})!=1:ap.error('入力ROOT間でn_fadc_clkが異なります')
    ROOT.gInterpreter.Declare(CPP)
    acc=ROOT.EricaFADCAccum.Accumulator(info[0][0]);used=[]
    for path,(_,entries,threshold) in zip(paths,info):
        left=-1 if args.max_events<0 else args.max_events-int(acc.events)
        if left==0:break
        if threshold>0:print(f'WARNING: {path}: save_threshold={threshold} ADU。保存されていない波形は積算できません。',file=sys.stderr)
        f=ROOT.TFile.Open(str(path))
        try:
            before=int(acc.events);acc.run(f.Get(raw_tree_path(path)),left)
            used.append({'path':str(path),'available_events':entries,'used_events':int(acc.events)-before,'save_threshold_ADU':threshold})
        finally:f.Close()
        print(path.name,used[-1]['used_events'],'events',flush=True)
    if not acc.events:raise ValueError('積算するイベントがありません')
    acc.finish()
    summary={'inputs':used,'events':int(acc.events),'max_events':args.max_events,'n_fadc_clk':info[0][0],
             'x_unit':'sample','y_unit':'ADU','sample_period_ns':20,
             'selection':'all stored waveforms; no pulse or charge selection; no baseline subtraction',
             'cathode':'already inverted by ReadData (1023 - ADC); no additional inversion',
             'channels':{}}
    for g in range(96):
        h=acc.get(g);expected=int(acc.count(g))*info[0][0]
        if h.Integral()!=expected:raise RuntimeError(f'{h.GetName()}: counts mismatch')
        summary['channels'][h.GetName()]={'waveforms':int(acc.count(g)),'sample_count':expected}
    output.parent.mkdir(parents=True,exist_ok=True)
    part=output.with_name(output.name+'.part')
    if part.exists():raise FileExistsError(f'{part}があります。別の処理が実行中でないか確認してください')
    f=ROOT.TFile(str(part),'CREATE')
    if not f or f.IsZombie():raise OSError(f'{part}を作成できません')
    try:
        f.mkdir('FADC_Accum').cd()
        for g in range(96):
            if acc.get(g).Write()<=0:raise OSError('ヒストグラムの保存に失敗しました')
        f.cd();ROOT.TObjString(json.dumps(summary,ensure_ascii=False)).Write('metadata_json')
    finally:f.Close()
    if f.TestBit(ROOT.TFile.kWriteError):raise OSError(f'{part}の書き込みに失敗しました')
    os.replace(part,output)
    print('wrote',output,':',acc.events,'events, 96 histograms',flush=True)
    if args.png_dir:write_pngs(output,args.png_dir.expanduser().resolve())


if __name__=='__main__':
    main()
