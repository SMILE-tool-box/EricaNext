#include "TOT_Skewness.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

#include <TF1.h>
#include <TGraph.h>
#include <TH1D.h>

TOT_Skewness::~TOT_Skewness() {}

ANLStatus TOT_Skewness::mod_define() {
  define_parameter("pitch_x_mm_per_strip", &mod_class::Pitch_X);
  define_parameter("pitch_y_mm_per_strip", &mod_class::Pitch_Y);
  define_parameter("pitch_z_mm_per_clock", &mod_class::Pitch_Z);
  define_parameter("clock_offset",         &mod_class::TPC_Clock_Offset);
  define_parameter("flag_pixel_cut",       &mod_class::Flag_Pixel_Cut);
  define_parameter("flag_save_cloud",      &mod_class::Flag_Save_Cloud);
  return AS_OK;
}

ANLStatus TOT_Skewness::mod_initialize() {
  get_module("ReadData", &RawData);

  if (!RawData->Get_Track_A() || !RawData->Get_Track_C())
    return AS_QUIT;

  TPC_Pitch[0]  = Pitch_X;
  TPC_Pitch[1]  = Pitch_Y;
  TPC_Pitch[2]  = Pitch_Z;
  TPC_STRIP_ALL = RawData->Get_Track_A()->GetXaxis()->GetNbins();
  TPC_DEPTH     = RawData->Get_Track_A()->GetYaxis()->GetNbins();

  if (exist_module("SaveFile")) {
    SaveFile* sf = nullptr;
    get_module_NC("SaveFile", &sf);
    sf->cd();
    sf->GetDirectory()->mkdir("TOT_Skewness");
    sf->GetDirectory()->cd("TOT_Skewness");

    tree_ = new TTree("tot_skewness", "TOT Skewness Analysis");
    tree_->Branch("Pe",          Pe,          "Pe[3]/D");
    tree_->Branch("Ve",          Ve,          "Ve[3]/D");
    tree_->Branch("Len",        &Len,        "Len/D");
    tree_->Branch("skewness_x", &skewness_x, "skewness_x/D");
    tree_->Branch("skewness_y", &skewness_y, "skewness_y/D");
    tree_->Branch("can",        &can,        "can/I");
    if (Flag_Save_Cloud) {
      tree_->Branch("cloud_x", &caq0);
      tree_->Branch("cloud_y", &caq1);
      tree_->Branch("cloud_z", &caq2);
    }

    h_flag_pixel_cut = new TH1F("h_flag_pixel_cut", "Pixel cut flag", 100, 0, 100);
    h_cut_pixel = new TH2F("h_cut_pixel", "Pixel cut map",
                            TPC_STRIP_ALL, 0, TPC_STRIP_ALL,
                            TPC_STRIP_ALL, 0, TPC_STRIP_ALL);
    h_cut_pixel->SetXTitle("Anode");
    h_cut_pixel->SetYTitle("Cathode");
    TPC_Map_xz = new TH2F("TPC_Map_xz", "TPC x-z map",
                           400, -200., 200., 400, -100., 500.);
    TPC_Map_yz = new TH2F("TPC_Map_yz", "TPC y-z map",
                           400, -200., 200., 400, -100., 500.);
  }

  define_evs("TOT_Pe_TRA");
  define_evs("TOT_Ve_TRA");
  define_evs("TOT_Len_TRA");
  define_evs("TOT_Skewness");

  return AS_OK;
}

ANLStatus TOT_Skewness::mod_analyze() {
  ClearEvent();
  TOT_Correction();

  if (NUM[0] == 0 || NUM[1] == 0) return AS_SKIP;

  TPC_Track_Recon();

  if (tree_) tree_->Fill();
  return AS_OK;
}

ANLStatus TOT_Skewness::mod_finalize() {
  return AS_OK;
}

//----------------------------------------------------------------------
void TOT_Skewness::ClearEvent() {
  for (int i = 0; i < 3; i++) {
    Pe[i] = static_cast<double>(0xffffffff);
    Ve[i] = static_cast<double>(0xffffffff);
  }
  Len = 0.; skewness_x = 0.; skewness_y = 0.;
  for (int i = 0; i < 3; i++) { Track_Min[i] = 1e6; Track_Max[i] = 1e6; }
  NUM[0] = NUM[1] = 0; can = 0;
  str_a.clear();   str_c.clear();
  corr_tot_a.clear(); corr_tot_c.clear();
  caq0.clear(); caq1.clear(); caq2.clear();
}

//----------------------------------------------------------------------
// TOT補正: ReadData::Hit ベクタから strip/clock を読み、
// ストリップごとに連続領域をまとめて補正済みクロック値を計算する。
//----------------------------------------------------------------------
void TOT_Skewness::TOT_Correction() {
  const auto& hits_a = RawData->Get_Anode_Hits();
  const auto& hits_c = RawData->Get_Cathode_Hits();

  std::vector<int> clk_max_a(TPC_STRIP_ALL, 0);
  std::vector<int> clk_min_a(TPC_STRIP_ALL, 1023);
  std::vector<int> clk_max_c(TPC_STRIP_ALL, 0);
  std::vector<int> clk_min_c(TPC_STRIP_ALL, 1023);

  // アノード: Hit ベクタをそのままイテレート
  for (const auto& h : hits_a) {
    int strip = h.strip, clk = h.clock;
    if (strip < 0 || strip >= TPC_STRIP_ALL) continue;
    if (clk_max_a[strip] != 0 && clk_max_a[strip] <= clk - 2) {
      corr_tot_a.push_back(tot_corr0(clk_max_a[strip], clk_min_a[strip]));
      str_a.push_back(strip);
      clk_min_a[strip] = clk;
    }
    if (clk_max_a[strip] <= clk) clk_max_a[strip] = clk;
    if (clk_min_a[strip] >= clk) clk_min_a[strip] = clk;
  }
  for (int s = 0; s < TPC_STRIP_ALL; s++) {
    if (clk_max_a[s] >= clk_min_a[s]) {
      corr_tot_a.push_back(tot_corr0(clk_max_a[s], clk_min_a[s]));
      str_a.push_back(s);
    }
  }

  // カソード
  for (const auto& h : hits_c) {
    int strip = h.strip, clk = h.clock;
    if (strip < 0 || strip >= TPC_STRIP_ALL) continue;
    if (clk_max_c[strip] != 0 && clk_max_c[strip] <= clk - 2) {
      corr_tot_c.push_back(tot_corr1(clk_max_c[strip], clk_min_c[strip]));
      str_c.push_back(strip);
      clk_min_c[strip] = clk;
    }
    if (clk_max_c[strip] <= clk) clk_max_c[strip] = clk;
    if (clk_min_c[strip] >= clk) clk_min_c[strip] = clk;
  }
  for (int s = 0; s < TPC_STRIP_ALL; s++) {
    if (clk_max_c[s] >= clk_min_c[s]) {
      corr_tot_c.push_back(tot_corr1(clk_max_c[s], clk_min_c[s]));
      str_c.push_back(s);
    }
  }

  NUM[0] = static_cast<int>(str_a.size());
  NUM[1] = static_cast<int>(str_c.size());
}

//----------------------------------------------------------------------
void TOT_Skewness::CalcSkewness() {
  if (caq0.empty() || caq1.empty()) return;

  double x_min = *std::min_element(caq0.begin(), caq0.end());
  double x_max = *std::max_element(caq0.begin(), caq0.end());
  double y_min = *std::min_element(caq1.begin(), caq1.end());
  double y_max = *std::max_element(caq1.begin(), caq1.end());

  int x_bin = static_cast<int>((x_max - x_min) / TPC_Pitch[0]) + 1;
  int y_bin = static_cast<int>((y_max - y_min) / TPC_Pitch[1]) + 1;
  if (x_bin <= 0) x_bin = 1;
  if (y_bin <= 0) y_bin = 1;

  TH1D* h_xz = new TH1D("", "", x_bin,
                          x_min - TPC_Pitch[0]/2., x_max + TPC_Pitch[0]/2.);
  TH1D* h_yz = new TH1D("", "", y_bin,
                          y_min - TPC_Pitch[1]/2., y_max + TPC_Pitch[1]/2.);
  h_xz->SetDirectory(nullptr);
  h_yz->SetDirectory(nullptr);

  for (double v : caq0) h_xz->Fill(v);
  for (double v : caq1) h_yz->Fill(v);

  double rise_a = 0., rise_c = 0.;
  for (int i = 0; i < x_bin; i++) if (h_xz->GetBinContent(i+1) > 0) rise_a++;
  for (int i = 0; i < y_bin; i++) if (h_yz->GetBinContent(i+1) > 0) rise_c++;

  if (rise_a > 0.) {
    double mean_x = (x_min + x_max) / 2.;
    double s3 = 0., s2 = 0.;
    for (int i = 0; i < x_bin; i++) {
      double dx  = h_xz->GetBinCenter(i+1) - mean_x;
      double tot = h_xz->GetBinContent(i+1);
      s3 += tot * dx*dx*dx / rise_a;
      s2 += tot * dx*dx    / rise_a;
    }
    skewness_x = (s2 > 0.) ? s3 / std::pow(s2, 1.5) : 0.;
  }

  if (rise_c > 0.) {
    double mean_y = (y_min + y_max) / 2.;
    double s3 = 0., s2 = 0.;
    for (int i = 0; i < y_bin; i++) {
      double dy  = h_yz->GetBinCenter(i+1) - mean_y;
      double tot = h_yz->GetBinContent(i+1);
      s3 += tot * dy*dy*dy / rise_c;
      s2 += tot * dy*dy    / rise_c;
    }
    skewness_y = (s2 > 0.) ? s3 / std::pow(s2, 1.5) : 0.;
  }

  // オフセット補正 (ref: 2020.07.28)
  skewness_x -= 0.0006029;
  skewness_y -= 0.002347;
  skewness_y *= 0.07857 / 0.08867;

  delete h_xz;
  delete h_yz;

  set_evs("TOT_Skewness");
}

//----------------------------------------------------------------------
int TOT_Skewness::Komtan_3D_Coincidence(
    std::vector<double>* acp0, std::vector<double>* acp1, std::vector<double>* acp2) {
  const double w = 4.;
  int acn = 0;

  acp0->reserve(10000);
  acp1->reserve(10000);
  acp2->reserve(10000);

  for (int i = 0; i < (int)corr_tot_a.size(); i++) {
    for (int j = 0; j < (int)corr_tot_c.size(); j++) {

      if (str_a[i]/384 != str_c[j]/384 || std::fabs(corr_tot_a[i] - corr_tot_c[j]) > w) continue;
      if (Flag_Pixel_Cut && PixelMask(str_a[i], str_c[j])) {
        if (h_flag_pixel_cut) h_flag_pixel_cut->Fill(0.5);
        continue;
      }
      acp0->push_back(str_a[i]);
      acp1->push_back(str_c[j]);
      acp2->push_back(corr_tot_a[i]);
      acn++;
    }
  }
  return acn;
}

//----------------------------------------------------------------------
int TOT_Skewness::ConvertCloudPosition(
    int n,
    std::vector<double>& acp0, std::vector<double>& acp1, std::vector<double>& acp2,
    std::vector<double>& out0,  std::vector<double>& out1,  std::vector<double>& out2) {
  out0.clear(); out1.clear(); out2.clear();
  int count = 0;
  for (int k = 0; k < n; k++) {
    if (acp0[k] >= 0 && acp0[k] < TPC_STRIP_ALL &&
        acp1[k] >= 0 && acp1[k] < TPC_STRIP_ALL &&
        acp2[k] >= 0 && acp2[k] < TPC_DEPTH) {
      int pa = (int)acp0[k]/384, pc = (int)acp1[k]/384;
      if(pa == 0 && pc == 0){
        out0.push_back(Get_TPC_XYZ(0, acp0[k]));// x, Anode
        out1.push_back(Get_TPC_XYZ(1, acp1[k]));// y,  Cathode
        out2.push_back(Get_TPC_XYZ(2, acp2[k]));// z,  Clock
      } else if(pa == 1 && pc == 1){
        out1.push_back(Get_TPC_XYZ(0, acp0[k]));// y, Anode
        out0.push_back(Get_TPC_XYZ(1, acp1[k]));// x,  Cathode
        out2.push_back(Get_TPC_XYZ(2, acp2[k]));// z,  Clock
      } else if(pa == 2 && pc == 2){
        out0.push_back(Get_TPC_XYZ(0, acp0[k]));// x, Anode
        out1.push_back(Get_TPC_XYZ(1, acp1[k]));// y,  Cathode
        out2.push_back(Get_TPC_XYZ(2, acp2[k]));// z,  Clock
      } else if(pa == 3 && pc == 3){
        out1.push_back(Get_TPC_XYZ(0, acp0[k]));// y, Anode
        out0.push_back(Get_TPC_XYZ(1, acp1[k]));// x,  Cathode
        out2.push_back(Get_TPC_XYZ(2, acp2[k]));// z,  Clock
      } else {
        continue;
      }
      count++;
    }
  }
  return count;
}

//----------------------------------------------------------------------
void TOT_Skewness::Calc_Len(
    std::vector<double>& p0, std::vector<double>& p1, std::vector<double>& p2) {
  Track_Min[0] = *std::min_element(p0.begin(), p0.end());
  Track_Min[1] = *std::min_element(p1.begin(), p1.end());
  Track_Min[2] = *std::min_element(p2.begin(), p2.end());
  Track_Max[0] = *std::max_element(p0.begin(), p0.end());
  Track_Max[1] = *std::max_element(p1.begin(), p1.end());
  Track_Max[2] = *std::max_element(p2.begin(), p2.end());

  Len = std::sqrt(
    std::pow(Track_Max[0] - Track_Min[0], 2.) +
    std::pow(Track_Max[1] - Track_Min[1], 2.) +
    std::pow(Track_Max[2] - Track_Min[2], 2.)
  );

  if (TPC_Map_xz) {
    TPC_Map_xz->Fill(Track_Min[0], Track_Min[2]);
    TPC_Map_xz->Fill(Track_Max[0], Track_Max[2]);
  }
  if (TPC_Map_yz) {
    TPC_Map_yz->Fill(Track_Min[1], Track_Min[2]);
    TPC_Map_yz->Fill(Track_Max[1], Track_Max[2]);
  }
  set_evs("TOT_Len_TRA");
}

//----------------------------------------------------------------------
void TOT_Skewness::TPC_Track_Recon() {
  std::vector<double> acp0, acp1, acp2;
  int acn = Komtan_3D_Coincidence(&acp0, &acp1, &acp2);
  if (acn == 0) return;

  can = ConvertCloudPosition(acn, acp0, acp1, acp2, caq0, caq1, caq2);
  if (can == 0) return;

  Calc_Len(caq0, caq1, caq2);
  CalcSkewness();

  //------- 散乱点決定 -------
  const bool sx_dominant = std::fabs(skewness_x) > std::fabs(skewness_y);

  if (sx_dominant) {
    double extremum = (skewness_x >= 0.)
      ? *std::min_element(caq0.begin(), caq0.end())
      : *std::max_element(caq0.begin(), caq0.end());
    std::vector<int> idx; std::vector<double> vals;
    for (int i = 0; i < (int)caq0.size(); i++) {
      if (std::fabs(caq0[i] - extremum) < DBL_EPSILON) {
        idx.push_back(i); vals.push_back(caq1[i]);
      }
    }
    int best = (idx.size() == 1) ? 0
      : (skewness_y >= 0.)
        ? (int)(std::min_element(vals.begin(), vals.end()) - vals.begin())
        : (int)(std::max_element(vals.begin(), vals.end()) - vals.begin());
    Pe[0] = caq0[idx[best]]; Pe[1] = caq1[idx[best]]; Pe[2] = caq2[idx[best]];
  } else {
    double extremum = (skewness_y >= 0.)
      ? *std::min_element(caq1.begin(), caq1.end())
      : *std::max_element(caq1.begin(), caq1.end());
    std::vector<int> idx; std::vector<double> vals;
    for (int i = 0; i < (int)caq1.size(); i++) {
      if (std::fabs(caq1[i] - extremum) < DBL_EPSILON) {
        idx.push_back(i); vals.push_back(caq0[i]);
      }
    }
    int best = (idx.size() == 1) ? 0
      : (skewness_x >= 0.)
        ? (int)(std::min_element(vals.begin(), vals.end()) - vals.begin())
        : (int)(std::max_element(vals.begin(), vals.end()) - vals.begin());
    Pe[0] = caq0[idx[best]]; Pe[1] = caq1[idx[best]]; Pe[2] = caq2[idx[best]];
  }
  set_evs("TOT_Pe_TRA");

  //------- 方向フィット -------
  const double width = 10.;  // [mm]

  auto build_sub = [&](int xy,
                        std::vector<double>& pos_sub,
                        std::vector<double>& z_sub,
                        std::vector<double>& pos_all) {
    for (int j = 0; j < (int)pos_all.size(); j++) {
      if (std::fabs(Pe[xy]-pos_all[j]) <= width && std::fabs(Pe[2]-caq2[j]) <= width) {
        pos_sub.push_back(pos_all[j]); z_sub.push_back(caq2[j]);
      }
    }
    if (pos_sub.size() == 1) {
      double min_dist = 1e9; int min_idx = -1;
      for (int j = 0; j < (int)pos_all.size(); j++) {
        double d = std::hypot(Pe[xy]-pos_all[j], Pe[2]-caq2[j]);
        if (d < min_dist && d > width) { min_dist = d; min_idx = j; }
      }
      if (min_idx >= 0) { pos_sub.push_back(pos_all[min_idx]); z_sub.push_back(caq2[min_idx]); }
    }
  };

  std::vector<double> x_sub, xz_sub, y_sub, yz_sub;
  build_sub(0, x_sub, xz_sub, caq0);
  build_sub(1, y_sub, yz_sub, caq1);

  if (x_sub.size() < 2 || y_sub.size() < 2) return;

  double track_grad[2] = {0., 0.};
  for (int xy = 0; xy < 2; xy++) {
    std::vector<double>& px  = (xy == 0) ? x_sub  : y_sub;
    std::vector<double>& pxz = (xy == 0) ? xz_sub : yz_sub;
    int n = (int)px.size();

    double sumx2=0, sumx=0, sumy=0, sumxy=0;
    for (int i = 0; i < n; i++) {
      sumx2 += px[i]*px[i]; sumx  += px[i];
      sumy  += pxz[i];      sumxy += px[i]*pxz[i];
    }
    double denom = n*sumx2 - sumx*sumx;
    double slope = (denom != 0.) ? (n*sumxy - sumx*sumy) / denom : 0.;

    TGraph* g = new TGraph(n, px.data(), pxz.data());
    TF1*    f = new TF1(Form("_totfit_%d", xy), "[0]*(x-[1])+[2]",
                         Pe[xy]-width, Pe[xy]+width);
    f->SetParameter(0, slope);
    f->SetParameter(1, Pe[xy]);
    f->SetParameter(2, Pe[2]);
    g->Fit(Form("_totfit_%d", xy), "Q", "", Pe[xy]-width, Pe[xy]+width);
    track_grad[xy] = f->GetParameter(0);
    f->Delete(); g->Delete();
  }

  set_evs("TOT_Ve_TRA");

  if (track_grad[0] == 0. || track_grad[1] == 0.) {
    double xl  = std::fabs(Track_Max[0] - Track_Min[0]);
    double yl  = std::fabs(Track_Max[1] - Track_Min[1]);
    double len = std::hypot(xl, yl);
    if (len == 0.) return;
    double sign = (std::hypot(Track_Min[0]-Pe[0], Track_Min[1]-Pe[1]) <
                   std::hypot(Track_Max[0]-Pe[0], Track_Max[1]-Pe[1])) ? 1. : -1.;
    Ve[0] = sign * (Track_Max[0] - Track_Min[0]) / len;
    Ve[1] = sign * (Track_Max[1] - Track_Min[1]) / len;
    Ve[2] = 0.;
  } else {
    int Z_up = 0, Z_dn = 0;
    for (double z : caq2) (z > Pe[2]) ? Z_up++ : Z_dn++;
    double sign = (Z_up > Z_dn) ? 1. : -1.;
    Ve[0] = sign / track_grad[0];
    Ve[1] = sign / track_grad[1];
    Ve[2] = sign;
  }
}

//----------------------------------------------------------------------
double TOT_Skewness::Get_TPC_XYZ(int xyz, int val) {
  if (xyz == 2) {
    return TPC_Pitch[2] * (double(val) - TPC_Clock_Offset) + 27.0;
  } else {
    if(xyz == 0 && val/384 == 0) {
      return TPC_Pitch[xyz] * (double(val%384) + 0.5); //x
    } else if(xyz == 0 && val/384 == 1) {
      return TPC_Pitch[xyz] * (double(val%384) + 0.5); //y
    } else if(xyz == 0 && val/384 == 2) {
      return TPC_Pitch[xyz] * (-double(val%384) - 0.5); //x
    } else if(xyz == 0 && val/384 == 3) {
      return TPC_Pitch[xyz] * (-double(val%384) - 0.5); //y
    } else if(xyz == 1 && val/384 == 0) {
      return TPC_Pitch[xyz] * (double(val%384) - 383.5); //y
    } else if(xyz == 1 && val/384 == 1) {
      return TPC_Pitch[xyz] * (-double(val%384) + 383.5); //x
    } else if(xyz == 1 && val/384 == 2) {
      return TPC_Pitch[xyz] * (-double(val%384) + 383.5); //y
    } else if(xyz == 1 && val/384 == 3) {
      return TPC_Pitch[xyz] * (double(val%384) - 383.5); //x
    }

  }
}

double TOT_Skewness::tot_corr0(int clock_max, int clock_min) {
  int tot = clock_max - clock_min;
  return clock_min + 1.28996 + 0.432939 * tot;
}

double TOT_Skewness::tot_corr1(int clock_max, int clock_min) {
  int tot = clock_max - clock_min;
  return clock_min + 0.69759 + 0.417947 * tot;
}

int TOT_Skewness::PixelMask(short anode, short cathode) {
  if (h_cut_pixel) h_cut_pixel->Fill(anode + 1, cathode + 1);
  return 0;
}

TOT_Skewness::TOT_Skewness(){
  Pitch_X          = 0.8;
  Pitch_Y          = 0.8;
  Pitch_Z          = 0.5;
  TPC_Clock_Offset = 90.0;
  Flag_Pixel_Cut   = false;
  Flag_Save_Cloud  = false;
  TPC_STRIP_ALL    = 384;
  TPC_DEPTH        = 1024;
  TPC_Pitch[0]     = 0.8;
  TPC_Pitch[1]     = 0.8;
  TPC_Pitch[2]     = 0.5;
  Len              = 0.;
  skewness_x       = 0.;
  skewness_y       = 0.;
  can              = 0;
  for (int i = 0; i < 3; i++) { Pe[i] = Ve[i] = 0.; }
  for (int i = 0; i < 3; i++) { Track_Min[i] = Track_Max[i] = 0.; }
  NUM[0] = NUM[1] = 0;
}
