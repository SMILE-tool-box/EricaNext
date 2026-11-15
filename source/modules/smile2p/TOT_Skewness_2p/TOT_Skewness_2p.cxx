#include "TOT_Skewness_2p.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <fstream>

#include <TF1.h>
#include <Math/MinimizerOptions.h>
#include <TGraph.h>
#include <TFitResultPtr.h>
#include <TH1D.h>

TOT_Skewness_2p::~TOT_Skewness_2p() {}

ANLStatus TOT_Skewness_2p::mod_define() {
  define_parameter("pitch_x_mm_per_strip", &mod_class::Pitch_X);
  define_parameter("pitch_y_mm_per_strip", &mod_class::Pitch_Y);
  define_parameter("drift_velocity_file",  &mod_class::DriftVelocityFile);
  define_parameter("flag_pixel_cut",       &mod_class::Flag_Pixel_Cut);
  define_parameter("flag_save_cloud",      &mod_class::Flag_Save_Cloud);
  return AS_OK;
}

ANLStatus TOT_Skewness_2p::mod_initialize() {
  get_module("ReadData_2p", &RawData);

  if (!RawData->Get_Track_A() || !RawData->Get_Track_C())
    return AS_QUIT;

  // The reference file was produced with ROOT 6.14, whose default fitter
  // for TGraph::Fit is TMinuit ("Minuit"); newer ROOT defaults to Minuit2,
  // which converges to slightly different slopes (~1e-11) -> different Ve.
  ROOT::Math::MinimizerOptions::SetDefaultMinimizer("Minuit");

  TPC_Pitch[0]  = Pitch_X;
  TPC_Pitch[1]  = Pitch_Y;
  TPC_STRIP_ALL = RawData->Get_Track_A()->GetXaxis()->GetNbins();
  TPC_DEPTH     = RawData->Get_Track_A()->GetYaxis()->GetNbins();

  // SMILE_tool_box TOT_Skewness_Analysis::Begin(), "Experiment" branch:
  // the drift-velocity file's first value IS the clock offset; the
  // remaining (hour, velocity) pairs are put into a TH1F with p-1 bins over
  // [0, last hour], bin i+1 <- velocity[i] (the last point lands in the
  // overflow bin). Process() then takes
  //   TPC_Pitch[2] = h_velocity->GetBinContent(FindBin(anode_time[0]/1e4/3600))
  // per event -- float bin contents included.
  {
    std::ifstream drift_data(DriftVelocityFile.c_str());
    if (!drift_data)  return AS_QUIT;
    drift_data >> TPC_Clock_Offset;
    std::vector<double> vy;
    double hour = 0., velocity = 0.;
    while (drift_data >> hour >> velocity)  vy.push_back(velocity);
    int p = (int)vy.size();
    h_velocity = new TH1F("h_velocity", "h_velocity", 1, 0, 1);
    h_velocity->SetDirectory(nullptr);
    h_velocity->SetBins(p - 1, 0, hour);
    for (int i = 0; i < p; i++)  h_velocity->SetBinContent(i + 1, vy[i]);
  }

  if (exist_module("SaveFile")) {
    SaveFile* sf = nullptr;
    get_module_NC("SaveFile", &sf);
    sf->cd();
    sf->GetDirectory()->mkdir("TOT_Skewness_2p");
    sf->GetDirectory()->cd("TOT_Skewness_2p");

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

  }

  define_evs("TOT_Pe_TRA");
  define_evs("TOT_Ve_TRA");
  define_evs("TOT_Len_TRA");
  define_evs("TOT_Skewness_2p");

  return AS_OK;
}

ANLStatus TOT_Skewness_2p::mod_analyze() {
  // SMILE_tool_box TOT_Skewness_Analysis::Process() (experiment branch)
  double vhour = RawData->Get_AnodeTime(0) / 1e4 / 3600.;
  TPC_Pitch[2] = h_velocity->GetBinContent(h_velocity->FindBin(vhour));

  ClearEvent();
  TOT_Correction();

  // SMILE_tool_box TOT_Skewness_Analysis::Process() never skips the chain;
  // with no track it just leaves Pe/Ve/Len/skewness at their cleared
  // (sentinel) values and still writes the TTree row.
  // SMILE_tool_box TPC_Track_Recon(): the track is only reconstructed when
  // HA_Calc_s2 produced a valid single-hit Pg this event (evs "Pg").
  bool pg_valid = is_evs_defined("HA2p_EgValid") && evs("HA2p_EgValid");
  if (NUM[0] != 0 && NUM[1] != 0 && pg_valid)
    TPC_Track_Recon();

  if (tree_) tree_->Fill();
  return AS_OK;
}

ANLStatus TOT_Skewness_2p::mod_finalize() {
  return AS_OK;
}

//----------------------------------------------------------------------
void TOT_Skewness_2p::ClearEvent() {
  fit_status[0] = fit_status[1] = 0;
  grad_err[0] = grad_err[1] = 0.;
  for (int i = 0; i < 3; i++) {
    Pe[i] = static_cast<double>(0xffffffff);
    Ve[i] = static_cast<double>(0xffffffff);
  }
  Len = 0.; skewness_x = 0.; skewness_y = 0.;
  for (int i = 0; i < 3; i++) { Track_Min[i] = 1e6; Track_Max[i] = 1e6; }
  NUM[0] = NUM[1] = 0; can = 0;
  str_a.clear();   str_c.clear();
  corr_tot_a.clear(); corr_tot_c.clear();
  TOTs_ax.clear(); TOTs_az.clear(); TOTs_cx.clear(); TOTs_cz.clear();
  caq0.clear(); caq1.clear(); caq2.clear();
}

//----------------------------------------------------------------------
// SMILE_tool_box TOT_Skewness_Analysis::TOT_Correction():
//   1) record the mm position of every raw hit strip (TOTs_ax/az/cx/cz),
//      used later by CalcSkewness() and by the Ve direction fit.
//   2) run the "miyamoto" TOT-correction algorithm per strip, collapsing
//      consecutive hit clocks on the same strip into a single TOT-corrected
//      clock value (str_a/corr_tot_a, str_c/corr_tot_c), used by the 3D
//      (anode x cathode x clock) coincidence search.
//----------------------------------------------------------------------
void TOT_Skewness_2p::TOT_Correction() {
  int min_a = RawData->GetMinAnode();
  int max_a = RawData->GetMaxAnode();
  int min_c = RawData->GetMinCathode();
  int max_c = RawData->GetMaxCathode();
  int min_d = RawData->GetMinClock();
  int max_d = RawData->GetMaxClock();

  std::vector<int> sta, cla, stc, clc;
  sta.reserve(10000); cla.reserve(10000);
  stc.reserve(10000); clc.reserve(10000);

  // Track_A/Track_C are value-filled (Fill(n_strip, clk)); with the default
  // -0.5..N-0.5 binning that means strip value V sits in ROOT bin V+1, so
  // GetBinContent needs (ai+1, clk+1) here while ai/clk stay raw 0-based
  // strip/clock values below (same fix as TPC_Calibration_2p::mod_analyze).
  for (int clk = min_d; clk <= max_d; clk++) {
    for (int ai = min_a; ai <= max_a; ai++) {
      if (RawData->Get_Track_A()->GetBinContent(ai+1, clk+1)) {
        int strip = ai, clock = clk;
        sta.push_back(strip);
        cla.push_back(clock);
        TOTs_ax.push_back(Get_TPC_XYZ(0, strip));
        TOTs_az.push_back(Get_TPC_XYZ(2, clock));
      }
    }
    for (int cj = min_c; cj <= max_c; cj++) {
      if (RawData->Get_Track_C()->GetBinContent(cj+1, clk+1)) {
        int strip = cj, clock = clk;
        stc.push_back(strip);
        clc.push_back(clock);
        TOTs_cx.push_back(Get_TPC_XYZ(1, strip));
        TOTs_cz.push_back(Get_TPC_XYZ(2, clock));
      }
    }
  }

  std::vector<int> clk_max_a(TPC_STRIP_ALL, 0), clk_min_a(TPC_STRIP_ALL, 1023);
  std::vector<int> clk_max_c(TPC_STRIP_ALL, 0), clk_min_c(TPC_STRIP_ALL, 1023);

  for (size_t i = 0; i < sta.size(); i++) {
    int s = sta[i], clk = cla[i];
    if (clk_max_a[s] != 0 && clk_max_a[s] <= clk - 2) {
      corr_tot_a.push_back(tot_corr0(clk_max_a[s], clk_min_a[s]));
      str_a.push_back(s);
      clk_min_a[s] = clk;
    }
    if (clk_max_a[s] <= clk) clk_max_a[s] = clk;
    if (clk_min_a[s] >= clk) clk_min_a[s] = clk;
  }
  for (int s = 0; s < TPC_STRIP_ALL; s++) {
    if (clk_max_a[s] >= clk_min_a[s]) {
      corr_tot_a.push_back(tot_corr0(clk_max_a[s], clk_min_a[s]));
      str_a.push_back(s);
    }
  }

  for (size_t i = 0; i < stc.size(); i++) {
    int s = stc[i], clk = clc[i];
    if (clk_max_c[s] != 0 && clk_max_c[s] <= clk - 2) {
      corr_tot_c.push_back(tot_corr1(clk_max_c[s], clk_min_c[s]));
      str_c.push_back(s);
      clk_min_c[s] = clk;
    }
    if (clk_max_c[s] <= clk) clk_max_c[s] = clk;
    if (clk_min_c[s] >= clk) clk_min_c[s] = clk;
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
// SMILE_tool_box CalcSkewness(): built from TOTs_ax/TOTs_cx, i.e. every
// raw hit strip's mm position -- NOT the 3D-coincidence-matched cloud.
//----------------------------------------------------------------------
void TOT_Skewness_2p::CalcSkewness() {
  if (TOTs_ax.empty() || TOTs_cx.empty()) return;

  double x_min = *std::min_element(TOTs_ax.begin(), TOTs_ax.end());
  double x_max = *std::max_element(TOTs_ax.begin(), TOTs_ax.end());
  double y_min = *std::min_element(TOTs_cx.begin(), TOTs_cx.end());
  double y_max = *std::max_element(TOTs_cx.begin(), TOTs_cx.end());

  int x_bin = (x_max - x_min) / TPC_Pitch[0] + 1;
  int y_bin = (y_max - y_min) / TPC_Pitch[1] + 1;
  if (x_bin <= 0) x_bin = 1;
  if (y_bin <= 0) y_bin = 1;

  TH1D* h_xz = new TH1D("", "", x_bin,
                          x_min - TPC_Pitch[0]/2., x_max + TPC_Pitch[0]/2.);
  TH1D* h_yz = new TH1D("", "", y_bin,
                          y_min - TPC_Pitch[1]/2., y_max + TPC_Pitch[1]/2.);
  h_xz->SetDirectory(nullptr);
  h_yz->SetDirectory(nullptr);

  for (double v : TOTs_ax) h_xz->Fill(v);
  for (double v : TOTs_cx) h_yz->Fill(v);

  double rise_a = 0., rise_c = 0.;
  for (int i = 0; i < x_bin; i++) if (h_xz->GetBinContent(i+1) > 0) rise_a = rise_a + 1;
  for (int i = 0; i < y_bin; i++) if (h_yz->GetBinContent(i+1) > 0) rise_c = rise_c + 1;

  // SMILE_tool_box CalcSkewness(): same expressions/order (pow, per-term
  // division, no zero guards -> 0/0 gives NaN exactly as the reference).
  double s3 = 0., s2 = 0.;
  double mean_x = (x_min + x_max) / 2.;
  for (int i = 0; i < x_bin; i++) {
    double this_x   = h_xz->GetBinCenter(i+1);
    double this_tot = h_xz->GetBinContent(i+1);
    s3 += this_tot * std::pow((this_x - mean_x), 3) / rise_a;
    s2 += this_tot * std::pow((this_x - mean_x), 2) / rise_a;
  }
  skewness_x = s3 / std::pow(s2, 3/2.);

  s3 = 0.; s2 = 0.;
  double mean_y = (y_min + y_max) / 2.;
  for (int i = 0; i < y_bin; i++) {
    double this_y   = h_yz->GetBinCenter(i+1);
    double this_tot = h_yz->GetBinContent(i+1);
    s3 += this_tot * std::pow((this_y - mean_y), 3) / rise_c;
    s2 += this_tot * std::pow((this_y - mean_y), 2) / rise_c;
  }
  skewness_y = s3 / std::pow(s2, 3/2.);

  // ref: 2020.07.28 offset correction
  skewness_x = skewness_x - 0.0006029;
  skewness_y = skewness_y - 0.002347;
  skewness_y = skewness_y * 0.07857 / 0.08867;

  delete h_xz;
  delete h_yz;

  set_evs("TOT_Skewness_2p");
}

//----------------------------------------------------------------------
int TOT_Skewness_2p::Komtan_3D_Coincidence(
    std::vector<double>* acp0, std::vector<double>* acp1, std::vector<double>* acp2) {
  const double w = 4.;
  int acn = 0;

  acp0->reserve(10000);
  acp1->reserve(10000);
  acp2->reserve(10000);

  for (int i = 0; i < (int)corr_tot_a.size(); i++) {
    for (int j = 0; j < (int)corr_tot_c.size(); j++) {

      if (std::fabs(corr_tot_a[i] - corr_tot_c[j]) > w) continue;
      if (Flag_Pixel_Cut && PixelMask(str_a[i], str_c[j])) continue;
      acp0->push_back(str_a[i]);
      acp1->push_back(str_c[j]);
      acp2->push_back(corr_tot_a[i]);
      acn++;
    }
  }
  return acn;
}

//----------------------------------------------------------------------
int TOT_Skewness_2p::ConvertCloudPosition(
    int n,
    std::vector<double>& acp0, std::vector<double>& acp1, std::vector<double>& acp2,
    std::vector<double>& out0,  std::vector<double>& out1,  std::vector<double>& out2) {
  out0.clear(); out1.clear(); out2.clear();
  int count = 0;
  for (int k = 0; k < n; k++) {
    if (acp0[k] >= 0 && acp0[k] < TPC_STRIP_ALL &&
        acp1[k] >= 0 && acp1[k] < TPC_STRIP_ALL &&
        acp2[k] >= 0 && acp2[k] < TPC_DEPTH) {
      out0.push_back(Get_TPC_XYZ(0, (int)acp0[k])); // X, Anode
      out1.push_back(Get_TPC_XYZ(1, (int)acp1[k])); // Y, Cathode
      out2.push_back(Get_TPC_XYZ(2, (int)acp2[k])); // Z, Clock (truncated, as in reference)
      count++;
    }
  }
  return count;
}

//----------------------------------------------------------------------
// SMILE_tool_box ConvertCloudPosition(vector<int>&, ...): mm-converts the
// FULL (non-coincidence-filtered) per-strip TOT-corrected lists, used only
// for the direction (Ve) fit.
//----------------------------------------------------------------------
void TOT_Skewness_2p::ConvertCloudPosition(
    std::vector<int>& x, std::vector<double>& xz,
    std::vector<int>& y, std::vector<double>& yz,
    std::vector<double>& cx, std::vector<double>& cxz,
    std::vector<double>& cy, std::vector<double>& cyz) {
  for (size_t n = 0; n < x.size(); n++) {
    if (x[n] >= 0 && x[n] < TPC_STRIP_ALL && xz[n] >= 0 && xz[n] < TPC_DEPTH) {
      cx.push_back(Get_TPC_XYZ(0, x[n]));
      cxz.push_back(Get_TPC_XYZ(2, (int)xz[n]));
    }
  }
  for (size_t n = 0; n < y.size(); n++) {
    if (y[n] >= 0 && y[n] < TPC_STRIP_ALL && yz[n] >= 0 && yz[n] < TPC_DEPTH) {
      cy.push_back(Get_TPC_XYZ(0, y[n]));  // ref uses axis 0 (X pitch) for both anode and cathode here
      cyz.push_back(Get_TPC_XYZ(2, (int)yz[n]));
    }
  }
}

//----------------------------------------------------------------------
void TOT_Skewness_2p::Calc_Len(
    std::vector<double>& p0, std::vector<double>& p1, std::vector<double>& p2) {
  // SMILE_tool_box Calc_Len(acp0,acp1,acp2): MIN/MAX are `short` (strip
  // index / TOT-corrected clock truncated), converted to mm with
  // Get_TPC_XYZ for Track_Min/Max; Len uses pitch * (MAX-MIN).
  short MIN[3] = {0, 0, 0}, MAX[3] = {0, 0, 0};
  if (!p0.empty()) {
    MIN[0] = (short)*std::min_element(p0.begin(), p0.end());
    MIN[1] = (short)*std::min_element(p1.begin(), p1.end());
    MIN[2] = (short)*std::min_element(p2.begin(), p2.end());
    MAX[0] = (short)*std::max_element(p0.begin(), p0.end());
    MAX[1] = (short)*std::max_element(p1.begin(), p1.end());
    MAX[2] = (short)*std::max_element(p2.begin(), p2.end());
  }
  for (int k = 0; k < 3; k++) {
    Track_Min[k] = Get_TPC_XYZ(k, MIN[k]);
    Track_Max[k] = Get_TPC_XYZ(k, MAX[k]);
  }

  Len = std::sqrt(
    std::pow(TPC_Pitch[0] * (MAX[0] - MIN[0]), 2) +
    std::pow(TPC_Pitch[1] * (MAX[1] - MIN[1]), 2) +
    std::pow(TPC_Pitch[2] * (MAX[2] - MIN[2]), 2)
  );

  set_evs("TOT_Len_TRA");
}

//----------------------------------------------------------------------
void TOT_Skewness_2p::TPC_Track_Recon() {
  std::vector<double> acp0, acp1, acp2;
  int acn = Komtan_3D_Coincidence(&acp0, &acp1, &acp2);

  // SMILE_tool_box calls Calc_Len() unconditionally, even with acn == 0
  // (std::min_element on an empty vector -- UB, but the reference file
  // shows it reads 0, i.e. Track_Min/Max = Get_TPC_XYZ(k, 0), Len = 0).
  Calc_Len(acp0, acp1, acp2);

  can = ConvertCloudPosition(acn, acp0, acp1, acp2, caq0, caq1, caq2);

  // full (unfiltered) anode/cathode position lists, used by the Ve fit
  std::vector<double> x, xz, y, yz;
  ConvertCloudPosition(str_a, corr_tot_a, str_c, corr_tot_c, x, xz, y, yz);

  // The reference file shows that with no 3D coincidence (can == 0) the
  // skewness is NOT updated (stale in s2t) while Track_Min/Max are, so the
  // binary returns before CalcSkewness() in that case.
  if (can == 0) return;
  CalcSkewness();

  //------- 散乱点決定 (SMILE_tool_box: choose extremum of the dominant-skewness axis) -------
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

  //------- 方向フィット (built from the FULL x/xz/y/yz lists, not caq) -------
  const double width = 10.;  // [mm]

  auto build_sub = [&](int xy,
                        std::vector<double>& pos_sub,
                        std::vector<double>& z_sub,
                        std::vector<double>& pos_all,
                        std::vector<double>& z_all) {
    for (int j = 0; j < (int)pos_all.size(); j++) {
      if (std::fabs(Pe[xy]-pos_all[j]) <= width && std::fabs(Pe[2]-z_all[j]) <= width) {
        pos_sub.push_back(pos_all[j]); z_sub.push_back(z_all[j]);
      }
    }
    if (pos_sub.size() == 1) {
      double min_dist = 1e9; int min_idx = -1;
      for (int j = 0; j < (int)pos_all.size(); j++) {
        double d = std::hypot(Pe[xy]-pos_all[j], Pe[2]-z_all[j]);
        if (d < min_dist && d > width) { min_dist = d; min_idx = j; }
      }
      if (min_idx >= 0) { pos_sub.push_back(pos_all[min_idx]); z_sub.push_back(z_all[min_idx]); }
    }
  };

  std::vector<double> x_sub, xz_sub, y_sub, yz_sub;
  if (x.size() > 1)  build_sub(0, x_sub, xz_sub, x, xz);
  if (y.size() > 1)  build_sub(1, y_sub, yz_sub, y, yz);

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
    // SMILE_tool_box: no zero-denominator guard; a vertical point set gives
    // 0/0 = NaN as the initial slope, which (with TMinuit) ends in a zero
    // gradient and hence the Track_Min/Max fallback for Ve below.
    double slope = (n*sumxy - sumx*sumy) / (n*sumx2 - sumx*sumx);

    TGraph* g = new TGraph(n, px.data(), pxz.data());
    TF1*    f = new TF1(Form("_totfit_%d", xy), "[0]*(x-[1])+[2]",
                         Pe[xy]-width, Pe[xy]+width);
    f->SetParameter(0, slope);
    f->SetParameter(1, Pe[xy]);
    f->SetParameter(2, Pe[2]);
    TFitResultPtr r = g->Fit(Form("_totfit_%d", xy), "Q", "", Pe[xy]-width, Pe[xy]+width);
    // ROOT >= 6.2x TMinuitMinimizer adds 100*(HESSE status) to the MIGRAD
    // status; the reference (ROOT 6.14) only reported the MIGRAD part
    // (observed values 0 / 4), so keep that part only.
    fit_status[xy] = ((int)r) % 100;
    grad_err[xy]   = f->GetParError(0);
    // The reference s2t (Ve_TRA_fit_status) shows that whenever a fit
    // failed (status 4) the Track_Min/Max fallback was used, i.e. the
    // binary treats a failed fit as zero gradient.
    track_grad[xy] = (fit_status[xy] == 0) ? f->GetParameter(0) : 0.;
    f->Delete(); g->Delete();
  }

  set_evs("TOT_Ve_TRA");

  if (track_grad[0] == 0. || track_grad[1] == 0.) {
    double xl  = std::fabs(Track_Max[0] - Track_Min[0]);
    double yl  = std::fabs(Track_Max[1] - Track_Min[1]);
    double len = std::sqrt(std::pow(xl, 2.0) + std::pow(yl, 2.0));
    // SMILE_tool_box: no len==0 guard (0/0 -> NaN, same as the reference)
    if (std::sqrt(std::pow(Track_Min[0]-Pe[0], 2.0) + std::pow(Track_Min[1]-Pe[1], 2.0)) <
        std::sqrt(std::pow(Track_Max[0]-Pe[0], 2.0) + std::pow(Track_Max[1]-Pe[1], 2.0))) {
      Ve[0] = (Track_Max[0] - Track_Min[0]) / len;
      Ve[1] = (Track_Max[1] - Track_Min[1]) / len;
    } else {
      Ve[0] = (Track_Min[0] - Track_Max[0]) / len;
      Ve[1] = (Track_Min[1] - Track_Max[1]) / len;
    }
    Ve[2] = 0.;
  } else {
    int Z_up = 0, Z_dn = 0;
    for (double z : TOTs_az) (z > Pe[2]) ? Z_up++ : Z_dn++;
    double sign = (Z_up > Z_dn) ? 1. : -1.;
    Ve[0] = sign / track_grad[0];
    Ve[1] = sign / track_grad[1];
    Ve[2] = sign;
  }
}

//----------------------------------------------------------------------
// SMILE_tool_box Get_TPC_XYZ(): single ASIC board set, 384 strips total
// (128 strips x 3 chips), no quadrant splitting.
//----------------------------------------------------------------------
double TOT_Skewness_2p::Get_TPC_XYZ(int xyz, int val) {
  if (xyz == 2) {
    return TPC_Pitch[2] * (double(val) - TPC_Clock_Offset) + 27.0;
  } else {
    double center_strip = double(int(128 * 3 / 2)) - 0.5;  // 191.5
    return TPC_Pitch[xyz] * (double(val) - center_strip);
  }
}

double TOT_Skewness_2p::tot_corr0(int clock_max, int clock_min) {
  int tot = clock_max - clock_min;
  return clock_min + 1.28996 + 0.432939 * tot;  // from surface experiment 20190701/Per02
}

double TOT_Skewness_2p::tot_corr1(int clock_max, int clock_min) {
  int tot = clock_max - clock_min;
  return clock_min + 0.69759 + 0.417947 * tot;  // from surface experiment 20190701/Per02
}

int TOT_Skewness_2p::PixelMask(short anode, short cathode) {
  // SMILE_tool_box's PixelMask() hardcodes balloon-flight-specific noisy
  // pixel time windows (day/hour/minute of the flight). Not applicable to
  // ground data; only reached when flag_pixel_cut is explicitly set.
  (void)anode;  (void)cathode;
  return 0;
}

TOT_Skewness_2p::TOT_Skewness_2p(){
  Pitch_X           = 0.8;
  Pitch_Y           = 0.8;
  DriftVelocityFile = "./drift_velocity.dat";
  Flag_Pixel_Cut    = false;
  Flag_Save_Cloud   = false;
  TPC_Clock_Offset  = 90.0;
  TPC_STRIP_ALL     = 384;
  TPC_DEPTH         = 1024;
  TPC_Pitch[0]      = 0.8;
  TPC_Pitch[1]      = 0.8;
  TPC_Pitch[2]      = 0.5;
  Len               = 0.;
  skewness_x        = 0.;
  skewness_y        = 0.;
  can               = 0;
  for (int i = 0; i < 3; i++) { Pe[i] = Ve[i] = 0.; }
  for (int i = 0; i < 3; i++) { Track_Min[i] = Track_Max[i] = 0.; }
  NUM[0] = NUM[1] = 0;
}
