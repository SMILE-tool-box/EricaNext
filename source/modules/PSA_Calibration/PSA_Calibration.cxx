#include "PSA_Calibration.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>

namespace {
std::vector<std::string> split_csv(const std::string& line){
  std::vector<std::string> out;
  std::string cell;
  std::stringstream ss(line);
  while(std::getline(ss, cell, ','))  out.push_back(cell);
  for(auto& c : out){
    c.erase(std::remove(c.begin(), c.end(), '\r'), c.end());
    c.erase(0, c.find_first_not_of(" \t"));
    c.erase(c.find_last_not_of(" \t") + 1);
  }
  return out;
}

// rows of a csv with a header, as {column: value}
std::vector<std::map<std::string, std::string>> read_csv(const std::string& path){
  std::vector<std::map<std::string, std::string>> rows;
  std::ifstream in(path);
  std::string line;
  if(!in || !std::getline(in, line))  return rows;
  std::vector<std::string> head = split_csv(line);
  while(std::getline(in, line)){
    if(line.find_first_not_of(" \t\r") == std::string::npos)  continue;
    std::vector<std::string> cell = split_csv(line);
    std::map<std::string, std::string> row;
    for(size_t k=0; k<head.size() && k<cell.size(); k++)  row[head[k]] = cell[k];
    rows.push_back(row);
  }
  return rows;
}

std::string board_file(const std::string& dir, const char* prefix, int id){
  char name[64];
  snprintf(name, sizeof(name), "%s_ha%02d.csv", prefix, id);
  return dir + "/" + name;
}
}

ANLStatus PSA_Calibration::mod_define(){
  define_parameter("position_dir", &mod_class::position_dir_);
  define_parameter("energy_dir",   &mod_class::energy_dir_);
  define_parameter("temp_coef",    &mod_class::temp_coef_);
  define_parameter("temp_ref",     &mod_class::temp_ref_);
  define_parameter("save_maps",    &mod_class::save_maps_);
  define_parameter("hg_max",       &mod_class::hg_max_);
  define_parameter("hg_bins",      &mod_class::hg_bins_);
  define_parameter("energy_max",   &mod_class::energy_max_);
  define_parameter("energy_bins",  &mod_class::energy_bins_);
  return AS_OK;
}

// 64 peaks per ch -> thin-plate spline to the pixel grid (grid_spect.py)
bool PSA_Calibration::LoadPosition(int i, const std::string& path){
  auto rows = read_csv(path);
  if(rows.empty())  return false;
  const int id = rd_->BoardID(i);
  bool all = true;
  int n_ties = 0;
  for(int ch=0; ch<N_CH; ch++){
    std::vector<double> x, y;
    std::vector<int> n_in, m_in;
    bool given = true;
    for(auto& r : rows){
      if(!r.count("ch") || !r.count("x") || !r.count("y") || std::stoi(r["ch"]) != ch)  continue;
      x.push_back(std::stod(r["x"]));
      y.push_back(std::stod(r["y"]));
      if(r.count("n") && r.count("m") && !r["n"].empty() && !r["m"].empty()){
        n_in.push_back(std::stoi(r["n"]));
        m_in.push_back(std::stoi(r["m"]));
      }
      else  given = false;
    }
    const int np = N_GRID * N_GRID;
    if((int)x.size() != np){
      std::cerr << "PSA_Calibration: " << PSA_Board::Name(id) << " ch" << ch << ": " << x.size()
                << " points in " << path << ", expected " << np << "; no position for this ch" << std::endl;
      all = false;
      continue;
    }
    std::vector<double> n(np), m(np);
    if(given){
      for(int k=0; k<np; k++){ n[k] = n_in[k]; m[k] = m_in[k]; }
    }
    else{
      // columns: sort by x, 8 groups of 8; in each group, sort by y.
      // Equal values (peaks at the same bin center) make the order ambiguous; numpy may
      // order them differently, so count them and ask for n,m (run_script/psa_position_nm.py).
      std::vector<int> xo(np);
      std::iota(xo.begin(), xo.end(), 0);
      std::stable_sort(xo.begin(), xo.end(), [&](int a, int b){ return x[a] < x[b]; });
      int n_tie = 0;
      for(int g=0; g<N_GRID; g++){
        if(g > 0 && x[xo[8*g - 1]] == x[xo[8*g]])  n_tie++;
        std::vector<int> grp(xo.begin() + 8*g, xo.begin() + 8*g + 8);
        std::stable_sort(grp.begin(), grp.end(), [&](int a, int b){ return y[a] < y[b]; });
        for(int k=0; k<N_GRID; k++){
          if(k > 0 && y[grp[k-1]] == y[grp[k]])  n_tie++;
          n[grp[k]] = g + 1;
          m[grp[k]] = k + 1;
        }
      }
      n_ties += n_tie;
    }
    if(!tps_[i][ch].Fit(x, y, n, m)){
      std::cerr << "PSA_Calibration: " << PSA_Board::Name(id) << " ch" << ch << ": thin-plate spline failed" << std::endl;
      all = false;
    }
  }
  if(n_ties)
    std::cerr << "PSA_Calibration: WARNING: " << PSA_Board::Name(id) << ": " << path << " has no n,m columns and " << n_ties
              << " pairs of peaks with equal x at a column border or equal y in a column; their (n, m) may differ"
              << " from grid_spect.py. Add n,m with run_script/psa_position_nm.py" << std::endl;
  return all;
}

bool PSA_Calibration::LoadEnergy(int i, const std::string& path){
  auto rows = read_csv(path);
  if(rows.empty())  return false;
  int n_ok = 0;
  for(auto& r : rows){
    if(!r.count("ch") || !r.count("n") || !r.count("m") || !r.count("p0") || !r.count("p1"))  continue;
    int ch = std::stoi(r["ch"]), n = std::stoi(r["n"]), m = std::stoi(r["m"]);
    if(ch < 0 || ch >= N_CH || n < 1 || n > N_GRID || m < 1 || m > N_GRID)  continue;
    int pix = 64*ch + 8*(n-1) + (m-1);
    p0_[i][pix] = std::stod(r["p0"]);
    p1_[i][pix] = std::stod(r["p1"]);
    n_ok++;
  }
  std::cout << "PSA_Calibration: " << PSA_Board::Name(rd_->BoardID(i)) << ": energy of " << n_ok << " pixels from " << path << std::endl;
  return n_ok > 0;
}

ANLStatus PSA_Calibration::mod_initialize(){
  if(!exist_module("PSA_ReadData") || !exist_module("PSA_CalcFADC") || !exist_module("PSA_CenterGravity"))
    return AS_QUIT;
  get_module("PSA_ReadData", &rd_);
  get_module("PSA_CalcFADC", &fadc_);
  get_module("PSA_CenterGravity", &cg_);
  nb_ = rd_->NBoard();

  tps_.assign(nb_, std::vector<ThinPlateSpline>(N_CH));
  x_corr_.assign(nb_, std::vector<double>(N_CH, NAN));
  y_corr_ = energy_ = x_corr_;
  pix_n_.assign(nb_, std::vector<int>(N_CH, -1));
  pix_m_ = pix_n_;
  has_energy_.assign(nb_, false);
  p0_.assign(nb_, std::vector<double>(N_CH * 64, NAN));
  p1_ = p0_;

  int n_pos = 0;
  for(int i=0; i<nb_; i++){
    const int id = rd_->BoardID(i);
    std::string pos = board_file(position_dir_, "position", id);
    if(!position_dir_.empty() && std::ifstream(pos)){
      LoadPosition(i, pos);
      n_pos++;
      std::cout << "PSA_Calibration: " << PSA_Board::Name(id) << ": position from " << pos << std::endl;
    }
    else
      std::cerr << "PSA_Calibration: " << PSA_Board::Name(id) << ": no " << pos << "; no pixel for this board" << std::endl;
    std::string en = board_file(energy_dir_, "energy", id);
    if(!energy_dir_.empty() && std::ifstream(en))
      has_energy_[i] = LoadEnergy(i, en);
  }
  if(n_pos == 0)
    std::cerr << "PSA_Calibration: WARNING: no position file in position_dir " << position_dir_ << std::endl;

  if(exist_module("SaveFile") && save_maps_){
    SaveFile* saveModule = nullptr;
    get_module_NC("SaveFile", &saveModule);
    saveModule->cd();
    saveModule->GetDirectory()->mkdir("PSA_Calibration");
    saveModule->GetDirectory()->cd("PSA_Calibration");
    corr_maps_.assign(nb_, std::vector<TH2D*>(N_CH, nullptr));
    for(int i=0; i<nb_; i++){
      const int id = rd_->BoardID(i);
      char name[48];
      for(int ch=0; ch<N_CH; ch++){
        snprintf(name, sizeof(name), "Corrected_ha%02d_ch%d", id, ch);
        corr_maps_[i][ch] = new TH2D(name, (std::string(name) + ";x_corr;y_corr").c_str(),
                                     160, -2., 11., 160, -2., 11.);
      }
      // hg spectrum of every pixel, for the energy calibration
      snprintf(name, sizeof(name), "PixelHG_ha%02d", id);
      pixel_spec_.push_back(new TH2D(name, (std::string(name) + ";pixel (64 ch + 8 (n-1) + m-1);hg [ADC x sample]").c_str(),
                                     N_CH * 64, -0.5, N_CH * 64 - 0.5, hg_bins_, 0, hg_max_));
      if(has_energy_[i]){
        snprintf(name, sizeof(name), "Energy_ha%02d", id);
        energy_spec_.push_back(new TH1D(name, (std::string(name) + ";energy [keV]").c_str(),
                                        energy_bins_, 0, energy_max_));
      }
      else
        energy_spec_.push_back(nullptr);
    }
  }
  return AS_OK;
}

ANLStatus PSA_Calibration::mod_analyze(){
  for(int i=0; i<nb_; i++){
    for(int ch=0; ch<N_CH; ch++){
      x_corr_[i][ch] = y_corr_[i][ch] = NAN;
      energy_[i][ch] = -1;
      pix_n_[i][ch] = pix_m_[i][ch] = -1;
    }
    if(!rd_->Present(i))  continue;
    for(int ch=0; ch<N_CH; ch++){
      if(!fadc_->Hit(i, ch) || !tps_[i][ch].Ready())  continue;
      double xc, yc;
      tps_[i][ch].Eval(cg_->XHG(i, ch), cg_->YHG(i, ch), xc, yc);
      x_corr_[i][ch] = xc;
      y_corr_[i][ch] = yc;
      int n = (int)std::floor(xc + 0.5), m = (int)std::floor(yc + 0.5);
      if(n >= 1 && n <= N_GRID && m >= 1 && m <= N_GRID){
        pix_n_[i][ch] = n;
        pix_m_[i][ch] = m;
      }
      int pix = Pixel(i, ch);
      double hg = fadc_->HG(i, ch);
      if(pix >= 0 && has_energy_[i] && !std::isnan(p1_[i][pix])){
        double hg_t = hg / (1. + temp_coef_ * (fadc_->TempMean(i) - temp_ref_));
        energy_[i][ch] = p0_[i][pix] + p1_[i][pix] * hg_t;
      }
      if(!corr_maps_.empty()){
        corr_maps_[i][ch]->Fill(xc, yc);
        if(pix >= 0)  pixel_spec_[i]->Fill(pix, hg);
        if(energy_spec_[i] && energy_[i][ch] >= 0)  energy_spec_[i]->Fill(energy_[i][ch]);
      }
    }
  }
  return AS_OK;
}

PSA_Calibration::PSA_Calibration(){
  position_dir_ = "";
  energy_dir_ = "";
  temp_coef_ = 0.;
  temp_ref_ = 25.;
  save_maps_ = true;
  hg_max_ = 400e3;
  hg_bins_ = 400;
  energy_max_ = 1000.;
  energy_bins_ = 1000;
}

PSA_Calibration::~PSA_Calibration(){}
