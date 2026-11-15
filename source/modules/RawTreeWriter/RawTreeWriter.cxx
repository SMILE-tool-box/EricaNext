#include "RawTreeWriter.h"
#include <iostream>

ANLStatus RawTreeWriter::InitializeTPC(){
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
  saveModule->GetDirectory()->mkdir("TPC_RawTreeWriter");
  saveModule->GetDirectory()->cd("TPC_RawTreeWriter");

  info_ = new TTree("run_info", "board IDs and writer settings");
  info_->Branch("anode_ids",   &anode_ids);
  info_->Branch("cathode_ids", &cathode_ids);
  info_->Branch("n_fadc_clk",  &n_fadc_clk,     "n_fadc_clk/I");
  info_->Branch("schema_version", &tpc_schema_version_, "schema_version/I");
  info_->Fill();

  tree_ = new TTree("raw", "event-synchronized TPC raw data");
  tree_->Branch("event_id", &event_id, "event_id/i");
  tree_->Branch("sevt_id",  &sevt_id,  "sevt_id/i");
  tree_->Branch("loopback", &loopback, "loopback/I");
  tree_->Branch("anode_time",   &anode_time);
  tree_->Branch("cathode_time", &cathode_time);
  {
    tree_->Branch("hit_a_strip", &hit_a_strip);
    tree_->Branch("hit_a_clock", &hit_a_clock);
    tree_->Branch("hit_c_strip", &hit_c_strip);
    tree_->Branch("hit_c_clock", &hit_c_clock);
    tree_->Branch("rec_a_board", &rec_a_board);
    tree_->Branch("rec_a_clock", &rec_a_clock);
    tree_->Branch("rec_c_board", &rec_c_board);
    tree_->Branch("rec_c_clock", &rec_c_clock);
  }
  {
    tree_->Branch("fadc_ch",   &fadc_ch);
    tree_->Branch("fadc_wave", &fadc_wave);
  }
  return AS_OK;
}

ANLStatus RawTreeWriter::AnalyzeTPC(){
  event_id = RawData->Get_EventID_coincidence();
  sevt_id  = RawData->Get_SEvtID();
  loopback = evs("IDcheck_Loopback") ? 1 : 0;

  anode_time.resize(n_anode);
  for(int i=0; i<n_anode; i++)    anode_time[i] = RawData->Get_AnodeTime(i);
  cathode_time.resize(n_cathode);
  for(int i=0; i<n_cathode; i++)  cathode_time[i] = RawData->Get_CathodeTime(i);

  {
    hit_a_strip.clear();  hit_a_clock.clear();  hit_c_strip.clear();  hit_c_clock.clear();
    for(const auto& h : RawData->Get_Anode_Hits())   { hit_a_strip.push_back(h.strip); hit_a_clock.push_back(h.clock); }
    for(const auto& h : RawData->Get_Cathode_Hits()) { hit_c_strip.push_back(h.strip); hit_c_clock.push_back(h.clock); }
    rec_a_board.clear();  rec_a_clock.clear();  rec_c_board.clear();  rec_c_clock.clear();
    for(const auto& r : RawData->Get_Anode_Records())   { rec_a_board.push_back(r.board); rec_a_clock.push_back(r.clock); }
    for(const auto& r : RawData->Get_Cathode_Records()) { rec_c_board.push_back(r.board); rec_c_clock.push_back(r.clock); }
  }

  {
    const int Nclk = n_fadc_clk;
    fadc_ch.clear();  fadc_wave.clear();
    std::vector<short> wave(4*Nclk);
    for(int side=0; side<2; side++){
      const std::vector<int>& ids = side ? cathode_ids : anode_ids;
      for(int i=0; i<(int)ids.size(); i++){
        RawData->Get_TPC_FADC(side == 1, i, wave.data());
        for(int ch=0; ch<4; ch++){
          const short* w = &wave[ch*Nclk];
          fadc_ch.push_back((short)(side*4*N_BOARD + 4*ids[i] + ch));
          fadc_wave.insert(fadc_wave.end(), w, w + Nclk);
        }
      }
    }
  }

  tree_->Fill();
  return AS_OK;
}

RawTreeWriter::RawTreeWriter(){}

RawTreeWriter::~RawTreeWriter(){
}

ANLStatus RawTreeWriter::InitializePSA(){
 if(!exist_module("PSA_ReadData") || !exist_module("SaveFile"))return AS_CRITICAL_ERROR_TO_FINALIZE;
 get_module("PSA_ReadData",&psa_reader_);
 SaveFile* save=nullptr;get_module_NC("SaveFile",&save);save->cd();
 auto* dir=save->GetDirectory()->mkdir("PSA_RawTreeWriter");dir->cd();
 psa_ids_=*psa_reader_->Get_BoardID();
 auto* info=new TTree("run_info","PSA raw board layout");info->Branch("board_ids",&psa_ids_);
 info->Branch("schema_version",&psa_schema_version_);info->Branch("n_sample",&psa_n_sample_);info->Branch("n_sig",&psa_n_sig_);info->Fill();
 psa_tree_=new TTree("raw","event-synchronized PSA raw frames");psa_row_.Branch(psa_tree_);
 return AS_OK;
}
ANLStatus RawTreeWriter::AnalyzePSA(){
 psa_row_=PSA_RawEvent{};
 psa_row_.trig=psa_reader_->Trig();psa_row_.sevt=psa_reader_->SEvtID();psa_row_.epoch=psa_reader_->Epoch();
 for(int i=0;i<psa_reader_->NBoard();++i){
  if(!psa_reader_->Present(i))continue;
  const auto& b=psa_reader_->Board(i);unsigned mask=0;
  for(int ch=0;ch<6;++ch)if(b.Hit(ch))mask|=1u<<ch;
  psa_row_.board.push_back(b.ID());psa_row_.board_epoch.push_back(b.Epoch());psa_row_.clk.push_back(b.Clk());psa_row_.time.push_back(b.Time());psa_row_.mask.push_back(mask);
  for(int k=0;k<6;++k)psa_row_.temp.push_back(b.TempRaw(k));
  for(int ch=0;ch<6;++ch)for(int k=0;k<50;++k)for(int s=0;s<8;++s)psa_row_.adc.push_back(b.ADC(ch,k,s));
 }
 psa_tree_->Fill();return AS_OK;
}

ANLStatus RawTreeWriter::mod_initialize(){
  if(!exist_module("SaveFile") || (!exist_module("ReadData") && !exist_module("PSA_ReadData"))){
    std::cerr << "RawTreeWriter: SaveFile and at least one raw reader are required" << std::endl;
    return AS_CRITICAL_ERROR_TO_FINALIZE;
  }
  if(exist_module("ReadData")){
    if(InitializeTPC()!=AS_OK) return AS_CRITICAL_ERROR_TO_FINALIZE;
  }
  if(exist_module("PSA_ReadData")){
    if(InitializePSA()!=AS_OK) return AS_CRITICAL_ERROR_TO_FINALIZE;
    if(RawData && !psa_reader_->SyncTPC()){
      std::cerr << "RawTreeWriter: PSA_ReadData.sync_tpc must be true with TPC" << std::endl;
      return AS_CRITICAL_ERROR_TO_FINALIZE;
    }
  }
  return AS_OK;
}

ANLStatus RawTreeWriter::mod_analyze(){
  if(RawData && psa_reader_ &&
     (RawData->Get_EventID_coincidence()!=psa_reader_->Trig() || RawData->Get_SEvtID()!=psa_reader_->SEvtID())){
    std::cerr << "RawTreeWriter: TPC and PSA are not synchronized" << std::endl;
    return AS_CRITICAL_ERROR_TO_FINALIZE;
  }
  if(RawData && AnalyzeTPC()!=AS_OK) return AS_CRITICAL_ERROR_TO_FINALIZE;
  if(psa_reader_ && AnalyzePSA()!=AS_OK) return AS_CRITICAL_ERROR_TO_FINALIZE;
  return AS_OK;
}
