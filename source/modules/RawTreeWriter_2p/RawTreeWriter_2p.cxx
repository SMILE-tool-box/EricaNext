#include "RawTreeWriter_2p.h"

ANLStatus RawTreeWriter_2p::mod_define(){
  define_parameter("save_threshold", &mod_class::save_threshold);
  define_parameter("base_window",    &mod_class::base_window);
  define_parameter("save_fadc",      &mod_class::save_fadc);
  define_parameter("save_hits",      &mod_class::save_hits);
  define_parameter("save_ha",        &mod_class::save_ha);
  return AS_OK;
}

ANLStatus RawTreeWriter_2p::mod_initialize(){
  if(!exist_module("SaveFile") || !exist_module("ReadData_2p"))
    return AS_QUIT;
  get_module("ReadData_2p", &RawData);

  n_anode   = RawData->Get_AnodeID()->size();
  n_cathode = RawData->Get_CathodeID()->size();
  n_ha      = RawData->Get_HA2pID()->size();
  anode_ids.assign(RawData->Get_AnodeID()->begin(), RawData->Get_AnodeID()->end());
  cathode_ids.assign(RawData->Get_CathodeID()->begin(), RawData->Get_CathodeID()->end());
  ha_ids.assign(RawData->Get_HA2pID()->begin(), RawData->Get_HA2pID()->end());

  SaveFile* saveModule = nullptr;
  get_module_NC("SaveFile", &saveModule);
  saveModule->cd();
  saveModule->GetDirectory()->mkdir("RawTreeWriter_2p");
  saveModule->GetDirectory()->cd("RawTreeWriter_2p");

  info_ = new TTree("run_info", "board IDs and writer settings");
  info_->Branch("anode_ids",   &anode_ids);
  info_->Branch("cathode_ids", &cathode_ids);
  info_->Branch("ha_ids",      &ha_ids);
  info_->Branch("save_threshold", &save_threshold, "save_threshold/D");
  info_->Branch("base_window",    &base_window,    "base_window/I");
  info_->Fill();

  tree_ = new TTree("raw", "event-synchronized raw data");
  tree_->Branch("event_id", &event_id, "event_id/i");
  tree_->Branch("sevt_id",  &sevt_id,  "sevt_id/i");
  tree_->Branch("loopback", &loopback, "loopback/I");
  tree_->Branch("anode_time",  &anode_time);
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
  if(save_ha){
    tree_->Branch("ha_adc", &ha_adc);
    tree_->Branch("ha_err", &ha_err);
  }
  return AS_OK;
}

ANLStatus RawTreeWriter_2p::mod_analyze(){
  event_id = RawData->Get_EventID();
  sevt_id  = RawData->Get_SEvtID();
  loopback = evs("IDcheck_Loopback") ? 1 : 0;

  anode_time.resize(n_anode);
  for(int i=0; i<n_anode; i++)  anode_time[i] = RawData->Get_AnodeTime(i);
  cathode_time.resize(n_cathode);
  for(int i=0; i<n_cathode; i++)  cathode_time[i] = RawData->Get_CathodeTime(i);

  hit_a_strip = RawData->Get_HitStripA();
  hit_a_clock = RawData->Get_HitClockA();
  hit_c_strip = RawData->Get_HitStripC();
  hit_c_clock = RawData->Get_HitClockC();
  rec_a_board = RawData->Get_RecordBoardA();
  rec_a_clock = RawData->Get_RecordClockA();
  rec_c_board = RawData->Get_RecordBoardC();
  rec_c_clock = RawData->Get_RecordClockC();

  if(save_fadc){
    const int Nclk = RawData->Get_FADC_clk();
    fadc_ch.clear();  fadc_wave.clear();
    std::vector<short> wave(4*Nclk);
    for(int side=0; side<2; side++){
      int nboard = side ? n_cathode : n_anode;
      for(int id=0; id<nboard; id++){
        RawData->Get_TPC_FADC(side == 1, id, wave.data());
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
          fadc_ch.push_back((short)(side*12 + 4*id + ch));
          fadc_wave.insert(fadc_wave.end(), w, w + Nclk);
        }
      }
    }
  }

  if(save_ha){
    ha_adc.resize(24*n_ha);  ha_err.resize(24*n_ha);
    short adc[24];  bool err[24];
    for(int i=0; i<n_ha; i++){
      RawData->Get_ADC_HA2p_Raw(i, adc, err);
      for(int k=0; k<24; k++){
        ha_adc[24*i+k] = adc[k];
        ha_err[24*i+k] = err[k] ? 1 : 0;
      }
    }
  }

  tree_->Fill();
  return AS_OK;
}

RawTreeWriter_2p::RawTreeWriter_2p(){
  save_threshold = 0.;   // off: on Per02 >99% of read-out channels exceed 15 ADC anyway
  base_window    = 30;
  save_fadc      = true;
  save_hits      = true;
  save_ha        = true;
}

RawTreeWriter_2p::~RawTreeWriter_2p(){
}
