#include "PSA_CalcFADC.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
// default of psa_decode.py when the board is not in the psa_param csv
constexpr double DEFAULT_TEMP_A = 3.925820834;
constexpr double DEFAULT_TEMP_B = 1230.29635;

std::vector<std::string> split_csv(const std::string& line){
  std::vector<std::string> out;
  std::string cell;
  std::stringstream ss(line);
  while(std::getline(ss, cell, ','))  out.push_back(cell);
  if(!line.empty() && line.back() == ',')  out.push_back("");
  for(auto& c : out){
    c.erase(std::remove(c.begin(), c.end(), '\r'), c.end());
    c.erase(0, c.find_first_not_of(" \t"));
    c.erase(c.find_last_not_of(" \t") + 1);
  }
  return out;
}
}

ANLStatus PSA_CalcFADC::mod_define(){
  define_parameter("pedestal_samples", &mod_class::pedestal_samples_);
  define_parameter("window_front",     &mod_class::window_front_);
  define_parameter("window_back",      &mod_class::window_back_);
  define_parameter("fixed_window",     &mod_class::fixed_window_);
  define_parameter("fixed_start",      &mod_class::fixed_start_);
  define_parameter("fixed_stop",       &mod_class::fixed_stop_);
  define_parameter("psa_param_file",   &mod_class::psa_param_file_);
  define_parameter("save_spectra",     &mod_class::save_spectra_);
  define_parameter("hg_max",           &mod_class::hg_max_);
  define_parameter("lg_max",           &mod_class::lg_max_);
  define_parameter("spec_bins",        &mod_class::spec_bins_);
  return AS_OK;
}

// TL..BR _A / _B of the row HA = ha<NN>, reversed onto the 6 temperature words (as get_temp_calibration)
bool PSA_CalcFADC::LoadTempParam(int id, double* a, double* b) const {
  for(int k=0; k<6; k++){ a[k] = DEFAULT_TEMP_A; b[k] = DEFAULT_TEMP_B; }
  if(psa_param_file_.empty())  return false;
  std::ifstream in(psa_param_file_);
  if(!in)  return false;
  std::string line;
  if(!std::getline(in, line))  return false;
  std::vector<std::string> head = split_csv(line);
  auto col = [&](const std::string& name){
    auto it = std::find(head.begin(), head.end(), name);
    return it == head.end() ? -1 : (int)(it - head.begin());
  };
  int c_ha = col("HA");
  if(c_ha < 0)  return false;
  char name[8];
  snprintf(name, sizeof(name), "ha%02d", id);
  static const char* keys[6] = {"TL", "TR", "ML", "MR", "BL", "BR"};
  while(std::getline(in, line)){
    std::vector<std::string> row = split_csv(line);
    if(c_ha >= (int)row.size())  continue;
    std::string ha = row[c_ha];
    std::transform(ha.begin(), ha.end(), ha.begin(), ::tolower);
    if(ha != name)  continue;
    for(int k=0; k<6; k++){
      int ca = col(std::string(keys[k]) + "_A"), cb = col(std::string(keys[k]) + "_B");
      if(ca >= 0 && ca < (int)row.size() && !row[ca].empty())  a[5 - k] = std::stod(row[ca]);
      if(cb >= 0 && cb < (int)row.size() && !row[cb].empty())  b[5 - k] = std::stod(row[cb]);
    }
    return true;
  }
  return false;
}

ANLStatus PSA_CalcFADC::mod_initialize(){
  if(!exist_module("PSA_ReadData"))
    return AS_QUIT;
  get_module("PSA_ReadData", &rd_);
  nb_ = rd_->NBoard();

  hit_.assign(nb_, std::vector<bool>(N_CH, false));
  sum_.assign(nb_, std::vector<std::vector<double>>(N_CH, std::vector<double>(N_SIG, 0.)));
  ped_ = sum_;
  peak_.assign(nb_, std::vector<int>(N_CH, 0));
  temp_.assign(nb_, std::vector<double>(6, 0.));
  temp_a_ = temp_;
  temp_b_ = temp_;
  temp_mean_.assign(nb_, 0.);
  temp_cal_.assign(nb_, false);

  if(!psa_param_file_.empty() && !std::ifstream(psa_param_file_))
    std::cerr << "PSA_CalcFADC: WARNING: no psa_param_file " << psa_param_file_ << std::endl;
  for(int i=0; i<nb_; i++){
    temp_cal_[i] = LoadTempParam(rd_->BoardID(i), temp_a_[i].data(), temp_b_[i].data());
    if(!temp_cal_[i])
      std::cerr << "PSA_CalcFADC: " << PSA_Board::Name(rd_->BoardID(i))
                << ": no temperature calibration in psa_param_file, default a, b used" << std::endl;
  }

  if(pedestal_samples_ < 1 || pedestal_samples_ > PSA_Board::N_SAMPLE){
    std::cerr << "PSA_CalcFADC: pedestal_samples must be 1-" << PSA_Board::N_SAMPLE << std::endl;
    return AS_QUIT;
  }

  if(exist_module("SaveFile") && save_spectra_){
    SaveFile* saveModule = nullptr;
    get_module_NC("SaveFile", &saveModule);
    saveModule->cd();
    saveModule->GetDirectory()->mkdir("PSA_CalcFADC");
    saveModule->GetDirectory()->cd("PSA_CalcFADC");
    for(int i=0; i<nb_; i++){
      char name[32];
      snprintf(name, sizeof(name), "HG_ha%02d", rd_->BoardID(i));
      spec_hg_.push_back(new TH2D(name, (std::string(name) + ";hg [ADC x sample];ch").c_str(),
                                  spec_bins_, 0, hg_max_, N_CH, -0.5, N_CH - 0.5));
      snprintf(name, sizeof(name), "LG_ha%02d", rd_->BoardID(i));
      spec_lg_.push_back(new TH2D(name, (std::string(name) + ";lg [ADC x sample];ch").c_str(),
                                  spec_bins_, 0, lg_max_, N_CH, -0.5, N_CH - 0.5));
    }
  }
  return AS_OK;
}

ANLStatus PSA_CalcFADC::mod_analyze(){
  const int NS = PSA_Board::N_SAMPLE;
  for(int i=0; i<nb_; i++){
    for(int ch=0; ch<N_CH; ch++)  hit_[i][ch] = false;
    if(!rd_->Present(i))  continue;
    const PSA_Board& b = rd_->Board(i);

    double tsum = 0;
    for(int k=0; k<6; k++){
      temp_[i][k] = ((double)b.TempRaw(k) - temp_b_[i][k]) / temp_a_[i][k];
      tsum += temp_[i][k];
    }
    temp_mean_[i] = tsum / 6;

    for(int ch=0; ch<N_CH; ch++){
      if(!b.Hit(ch))  continue;
      hit_[i][ch] = true;

      double w[PSA_Board::N_SAMPLE][N_SIG];
      for(int s=0; s<N_SIG; s++){
        double acc = 0;
        for(int r=0; r<pedestal_samples_; r++)  acc += b.ADC(ch, r, s);
        ped_[i][ch][s] = acc / pedestal_samples_;
        for(int r=0; r<NS; r++)  w[r][s] = b.ADC(ch, r, s) - ped_[i][ch][s];
      }

      int best = 0;
      double best_val = -1e30;
      for(int r=0; r<NS; r++){
        double v = w[r][4] + w[r][5] + w[r][6] + w[r][7];
        if(v > best_val){ best_val = v; best = r; }
      }
      peak_[i][ch] = best;

      int ws, we;
      if(fixed_window_){ ws = fixed_start_; we = fixed_stop_; }
      else{ ws = std::max(0, best - window_front_); we = std::min(NS - 1, best + window_back_); }
      ws = std::max(0, std::min(NS - 1, ws));
      we = std::max(0, std::min(NS - 1, we));

      for(int s=0; s<N_SIG; s++){
        double acc = 0;
        for(int r=ws+1; r<=we; r++)  acc += 0.5 * (w[r-1][s] + w[r][s]);
        sum_[i][ch][s] = acc;
      }

      if(!spec_hg_.empty()){
        spec_hg_[i]->Fill(HG(i, ch), ch);
        spec_lg_[i]->Fill(LG(i, ch), ch);
      }
    }
  }
  return AS_OK;
}

PSA_CalcFADC::PSA_CalcFADC(){
  pedestal_samples_ = 23;
  window_front_ = 4;
  window_back_  = 9;
  fixed_window_ = false;
  fixed_start_  = 25;
  fixed_stop_   = 41;
  psa_param_file_ = "";
  save_spectra_ = true;
  hg_max_ = 400e3;
  lg_max_ = 100e3;
  spec_bins_ = 400;
}

PSA_CalcFADC::~PSA_CalcFADC(){}
