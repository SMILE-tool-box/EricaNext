#include "PSA_Event.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
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
}

ANLStatus PSA_Event::mod_define(){
  define_parameter("geometry_file", &mod_class::geometry_file_);
  define_parameter("energy_min",    &mod_class::energy_min_);
  define_parameter("save_hists",    &mod_class::save_hists_);
  return AS_OK;
}

// board,ch,x0,y0,z0,ux,uy,uz,vx,vy,vz,pitch; lines starting with # are comments
bool PSA_Event::LoadGeometry(){
  std::ifstream in(geometry_file_);
  if(!in){
    std::cerr << "PSA_Event: WARNING: no geometry_file " << geometry_file_ << "; no hit has a position" << std::endl;
    return false;
  }
  static const char* cols[12] = {"board", "ch", "x0", "y0", "z0", "ux", "uy", "uz", "vx", "vy", "vz", "pitch"};
  std::string line;
  std::vector<std::string> head;
  while(std::getline(in, line)){
    if(line.empty() || line[0] == '#')  continue;
    head = split_csv(line);
    break;
  }
  std::map<std::string, int> idx;
  for(int k=0; k<(int)head.size(); k++)  idx[head[k]] = k;
  for(auto c : cols)
    if(!idx.count(c)){
      std::cerr << "PSA_Event: geometry_file " << geometry_file_ << " has no column " << c << std::endl;
      return false;
    }
  int n_rows = 0;
  while(std::getline(in, line)){
    if(line.find_first_not_of(" \t\r") == std::string::npos || line[0] == '#')  continue;
    std::vector<std::string> cell = split_csv(line);
    auto get = [&](const char* c){ return std::stod(cell.at(idx[c])); };
    int board = (int)get("board"), ch = (int)get("ch");
    int i = rd_->Index(board);
    if(i < 0 || ch < 0 || ch >= PSA_Board::N_CH)  continue;   // board not in this run
    Block& b = geo_[i][ch];
    b.p0[0] = get("x0");  b.p0[1] = get("y0");  b.p0[2] = get("z0");
    b.u[0]  = get("ux");  b.u[1]  = get("uy");  b.u[2]  = get("uz");
    b.v[0]  = get("vx");  b.v[1]  = get("vy");  b.v[2]  = get("vz");
    b.pitch = get("pitch");
    b.ok = true;
    n_rows++;
  }
  std::cout << "PSA_Event: geometry of " << n_rows << " blocks of this run from " << geometry_file_ << std::endl;
  for(int i=0; i<nb_; i++){
    int n = 0;
    for(int ch=0; ch<PSA_Board::N_CH; ch++)  n += geo_[i][ch].ok;
    if(n < PSA_Board::N_CH)
      std::cerr << "PSA_Event: " << PSA_Board::Name(rd_->BoardID(i)) << ": geometry of " << n << " of "
                << PSA_Board::N_CH << " ch; hits in the others are not used" << std::endl;
  }
  return n_rows > 0;
}

ANLStatus PSA_Event::mod_initialize(){
  if(!exist_module("PSA_ReadData") || !exist_module("PSA_CalcFADC") || !exist_module("PSA_Calibration")){
    std::cerr << "PSA_Event: needs PSA_ReadData, PSA_CalcFADC and PSA_Calibration before it" << std::endl;
    return AS_QUIT;
  }
  get_module("PSA_ReadData", &rd_);
  get_module("PSA_CalcFADC", &fadc_);
  get_module("PSA_Calibration", &cal_);
  nb_ = rd_->NBoard();
  geo_.assign(nb_, std::vector<Block>(PSA_Board::N_CH));
  LoadGeometry();

  define_evs("PSA_SingleHit");
  define_evs("PSA_MultiHit");
  define_evs("PSA_NoHits");

  if(exist_module("SaveFile") && save_hists_){
    SaveFile* saveModule = nullptr;
    get_module_NC("SaveFile", &saveModule);
    saveModule->cd();
    saveModule->GetDirectory()->mkdir("PSA_Event");
    saveModule->GetDirectory()->cd("PSA_Event");
    n_hits_ = new TH1D("NHits", "used PSA hits per event;hits;events", 20, -0.5, 19.5);
    single_spec_ = new TH1D("SingleHitEnergy", "PSA single-hit events;energy [keV];events", 1000, 0, 1000);
  }
  return AS_OK;
}

ANLStatus PSA_Event::mod_analyze(){
  det_eng_.clear();  det_x_.clear();  det_y_.clear();  det_z_.clear();
  det_board_.clear();  det_pixel_.clear();
  for(int i=0; i<nb_; i++){
    if(!rd_->Present(i))  continue;
    for(int ch=0; ch<PSA_Board::N_CH; ch++){
      if(!fadc_->Hit(i, ch))  continue;
      int pix = cal_->Pixel(i, ch);
      double e = cal_->Energy(i, ch);
      if(pix < 0 || e <= energy_min_)  continue;
      const Block& b = geo_[i][ch];
      if(!b.ok){ n_no_geometry_++;  continue; }
      double du = b.pitch * (cal_->PixN(i, ch) - 4.5), dv = b.pitch * (cal_->PixM(i, ch) - 4.5);
      det_eng_.push_back(e);
      det_x_.push_back(b.p0[0] + du * b.u[0] + dv * b.v[0]);
      det_y_.push_back(b.p0[1] + du * b.u[1] + dv * b.v[1]);
      det_z_.push_back(b.p0[2] + du * b.u[2] + dv * b.v[2]);
      det_board_.push_back(rd_->BoardID(i));
      det_pixel_.push_back(pix);
    }
  }
  const size_t n = det_eng_.size();
  if(n == 0)       set_evs("PSA_NoHits");
  else if(n == 1)  set_evs("PSA_SingleHit");
  else             set_evs("PSA_MultiHit");
  if(n_hits_)  n_hits_->Fill(n);
  if(single_spec_ && n == 1)  single_spec_->Fill(det_eng_[0]);
  return AS_OK;
}

ANLStatus PSA_Event::mod_finalize(){
  if(n_no_geometry_)
    std::cout << "PSA_Event: " << n_no_geometry_ << " hits with energy were not used (block not in geometry_file)" << std::endl;
  return AS_OK;
}

PSA_Event::PSA_Event(){
  geometry_file_ = "";
  energy_min_ = 0.;
  save_hists_ = true;
}

PSA_Event::~PSA_Event(){}
