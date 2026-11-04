#include "RawTreeWriter_sm3.h"

ANLStatus RawTreeWriter_sm3::mod_define(){
  define_parameter("save_threshold", &mod_class::save_threshold);
  define_parameter("base_window",    &mod_class::base_window);
  define_parameter("save_fadc",      &mod_class::save_fadc);
  define_parameter("save_hits",      &mod_class::save_hits);
  return AS_OK;
}

ANLStatus RawTreeWriter_sm3::mod_initialize(){
  if(!exist_module("SaveFile") || !exist_module("ReadData"))
    return AS_QUIT;
  get_module("ReadData", &RawData);

  n_anode   = RawData->Get_AnodeID()->size();
  n_cathode = RawData->Get_CathodeID()->size();
  anode_ids.assign(RawData->Get_AnodeID()->begin(), RawData->Get_AnodeID()->end());
  cathode_ids.assign(RawData->Get_CathodeID()->begin(), RawData->Get_CathodeID()->end());
  n_fadc_clk = RawData->Get_FADC_clk();
  for(int b : anode_ids)    if(b < 0 || b >= N_BOARD)  return AS_QUIT;
  for(int b : cathode_ids)  if(b < 0 || b >= N_BOARD)  return AS_QUIT;

  SaveFile* saveModule = nullptr;
  get_module_NC("SaveFile", &saveModule);
  saveModule->cd();
  saveModule->GetDirectory()->mkdir("RawTreeWriter_sm3");
  saveModule->GetDirectory()->cd("RawTreeWriter_sm3");

  info_ = new TTree("run_info", "board IDs and writer settings");
  info_->Branch("anode_ids",   &anode_ids);
  info_->Branch("cathode_ids", &cathode_ids);
  info_->Branch("n_fadc_clk",  &n_fadc_clk,     "n_fadc_clk/I");
  info_->Branch("save_threshold", &save_threshold, "save_threshold/D");
  info_->Branch("base_window",    &base_window,    "base_window/I");
  info_->Fill();

  tree_ = new TTree("raw", "event-synchronized TPC raw data");
  tree_->Branch("event_id", &event_id, "event_id/i");
  tree_->Branch("sevt_id",  &sevt_id,  "sevt_id/i");
  tree_->Branch("loopback", &loopback, "loopback/I");
  tree_->Branch("anode_time",   &anode_time);
  tree_->Branch("cathode_time", &cathode_time);
  if(save_hits){
    tree_->Branch("hit_a_strip", &hit_a_strip);
    tree_->Branch("hit_a_clock", &hit_a_clock);
    tree_->Branch("hit_c_strip", &hit_c_strip);
    tree_->Branch("hit_c_clock", &hit_c_clock);
    tree_->Branch("rec_a_board", &rec_a_board);
    tree_->Branch("rec_a_clock", &rec_a_clock);
    tree_->Branch("rec_c_board", &rec_c_board);
    tree_->Branch("rec_c_clock", &rec_c_clock);
  }
  if(save_fadc){
    tree_->Branch("fadc_ch",   &fadc_ch);
    tree_->Branch("fadc_wave", &fadc_wave);
  }
  return AS_OK;
}

ANLStatus RawTreeWriter_sm3::mod_analyze(){
  event_id = RawData->Get_EventID_coincidence();
  sevt_id  = RawData->Get_SEvtID();
  loopback = evs("IDcheck_Loopback") ? 1 : 0;

  anode_time.resize(n_anode);
  for(int i=0; i<n_anode; i++)    anode_time[i] = RawData->Get_AnodeTime(i);
  cathode_time.resize(n_cathode);
  for(int i=0; i<n_cathode; i++)  cathode_time[i] = RawData->Get_CathodeTime(i);

  if(save_hits){
    hit_a_strip.clear();  hit_a_clock.clear();  hit_c_strip.clear();  hit_c_clock.clear();
    for(const auto& h : RawData->Get_Anode_Hits())   { hit_a_strip.push_back(h.strip); hit_a_clock.push_back(h.clock); }
    for(const auto& h : RawData->Get_Cathode_Hits()) { hit_c_strip.push_back(h.strip); hit_c_clock.push_back(h.clock); }
    rec_a_board.clear();  rec_a_clock.clear();  rec_c_board.clear();  rec_c_clock.clear();
    for(const auto& r : RawData->Get_Anode_Records())   { rec_a_board.push_back(r.board); rec_a_clock.push_back(r.clock); }
    for(const auto& r : RawData->Get_Cathode_Records()) { rec_c_board.push_back(r.board); rec_c_clock.push_back(r.clock); }
  }

  if(save_fadc){
    const int Nclk = n_fadc_clk;
    fadc_ch.clear();  fadc_wave.clear();
    std::vector<short> wave(4*Nclk);
    for(int side=0; side<2; side++){
      const std::vector<int>& ids = side ? cathode_ids : anode_ids;
      for(int i=0; i<(int)ids.size(); i++){
        RawData->Get_TPC_FADC(side == 1, i, wave.data());
        for(int ch=0; ch<4; ch++){
          const short* w = &wave[ch*Nclk];
          double mean = 0;
          for(int k=0; k<base_window; k++)  mean += w[k];
          mean /= base_window;
          short wmax = w[0];
          bool all_zero = true;
          for(int k=0; k<Nclk; k++){
            if(w[k] > wmax) wmax = w[k];
            if(w[k] != 0)   all_zero = false;
          }
          if(all_zero)  continue;   // board without data in this event
          if(save_threshold > 0 && wmax - mean <= save_threshold)  continue;
          fadc_ch.push_back((short)(side*4*N_BOARD + 4*ids[i] + ch));
          fadc_wave.insert(fadc_wave.end(), w, w + Nclk);
        }
      }
    }
  }

  tree_->Fill();
  return AS_OK;
}

RawTreeWriter_sm3::RawTreeWriter_sm3(){
  save_threshold = 0.;   // off
  base_window    = 30;
  save_fadc      = true;
  save_hits      = true;
}

RawTreeWriter_sm3::~RawTreeWriter_sm3(){
}
