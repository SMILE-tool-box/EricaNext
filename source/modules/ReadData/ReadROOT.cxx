#include "ReadData.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

ANLStatus ReadData::InitializeROOT(){
  try {
    root_input_.reset(TFile::Open(root_file_.c_str(), "READ"));
    if(!root_input_ || root_input_->IsZombie()) throw std::runtime_error("cannot open ROOT input");
    auto* info=dynamic_cast<TTree*>(root_input_->Get("TPC_RawTreeWriter/run_info"));
    root_tree_=dynamic_cast<TTree*>(root_input_->Get("TPC_RawTreeWriter/raw"));
    if(!root_tree_){
      info=dynamic_cast<TTree*>(root_input_->Get("RawTreeWriter_sm3/run_info"));
      root_tree_=dynamic_cast<TTree*>(root_input_->Get("RawTreeWriter_sm3/raw"));
    }
    if(root_input_->Get("TPC_RawTreeWriter/raw") && root_input_->Get("RawTreeWriter_sm3/raw"))
      throw std::runtime_error("ambiguous input: both new and legacy TPC trees exist");
    if(!info || !root_tree_ || info->GetEntries()!=1) throw std::runtime_error("missing raw/run_info");
    std::vector<int> *a=nullptr,*c=nullptr; int n=0, version=0; double threshold=0;
    auto bindInfo=[&](const char* name, auto* address){
      if(info->SetBranchAddress(name,address)<0) throw std::runtime_error(std::string("invalid run_info branch: ")+name);
    };
    bindInfo("anode_ids",&a);bindInfo("cathode_ids",&c);
    bindInfo("n_fadc_clk",&n);
    if(info->GetBranch("schema_version")) bindInfo("schema_version",&version);
    else bindInfo("save_threshold",&threshold);
    if(info->GetEntry(0)<=0 || !a || !c || n!=512 || threshold!=0 || (info->GetBranch("schema_version") && version!=2))
      throw std::runtime_error("reconstruction requires complete raw waveforms (schema 2 or unfiltered legacy ROOT)");
    N_anode=*a;N_cathode=*c;
    info->ResetBranchAddresses(); delete a;delete c;
    for(const auto& ids : {N_anode,N_cathode}){
      std::set<int> unique;
      for(int id:ids) if(id<0 || id>=12 || !unique.insert(id).second) throw std::runtime_error("invalid board IDs");
    }
    if(N_anode.empty() && N_cathode.empty()) throw std::runtime_error("no TPC boards");
    auto bind=[&](const char* name, auto* address){
      if(root_tree_->SetBranchAddress(name,address)<0) throw std::runtime_error(std::string("required branch missing or invalid: ")+name);
    };
    bind("event_id",&event_id_coincidence);bind("sevt_id",&sevt_id_);bind("loopback",&root_loopback_);
    bind("anode_time",&root_at_);bind("cathode_time",&root_ct_);
    bind("hit_a_strip",&root_as_);bind("hit_a_clock",&root_ac_);
    bind("hit_c_strip",&root_cs_);bind("hit_c_clock",&root_cc_);
    bind("rec_a_board",&root_ab_);bind("rec_a_clock",&root_ar_);
    bind("rec_c_board",&root_cb_);bind("rec_c_clock",&root_cr_);
    bind("fadc_ch",&root_ch_);bind("fadc_wave",&root_wave_);
    if(root_first_entry_<0 || root_first_entry_>=root_tree_->GetEntries()) throw std::runtime_error("root_first_entry out of range");
    root_entry_=root_first_entry_;
    define_evs("IDcheck_Loopback");
    if(!N_anode.empty()) define_evs("Anode_Data");
    if(!N_cathode.empty()) define_evs("Cathode_Data");
    TPC_Only=true;
    return AS_OK;
  } catch(const std::exception& e){std::cerr<<"ReadData: "<<root_file_<<": "<<e.what()<<std::endl;return AS_CRITICAL_ERROR_TO_FINALIZE;}
}

ANLStatus ReadData::ReadROOT(){
  if(root_entry_>=root_tree_->GetEntries()) return AS_QUIT_ALL;
  try {
    if(root_tree_->GetEntry(root_entry_++)<=0) throw std::runtime_error("failed reading event");
    if(root_at_->size()!=N_anode.size() || root_ct_->size()!=N_cathode.size()) throw std::runtime_error("board time size mismatch");
    std::fill_n(root_fadc_,96*512,0);
    if(root_wave_->size()!=root_ch_->size()*512) throw std::runtime_error("waveform size mismatch");
    std::set<int> seen;
    for(size_t i=0;i<root_ch_->size();++i){
      int ch=root_ch_->at(i);
      if(ch<0 || ch>=96 || !seen.insert(ch).second) throw std::runtime_error("invalid FADC channel");
      const auto& ids=ch<48 ? N_anode : N_cathode;
      if(std::find(ids.begin(),ids.end(),(ch%48)/4)==ids.end()) throw std::runtime_error("FADC board missing from run_info");
      for(int k=0;k<512;++k){auto v=root_wave_->at(i*512+k);if(v<0 || v>1023) throw std::runtime_error("invalid ADC");root_fadc_[ch*512+k]=v;}
    }
    auto hits=[](const auto* strips,const auto* clocks, auto& out,TH2C* hist,int& lo,int& hi,int& tlo,int& thi){
      if(strips->size()!=clocks->size()) throw std::runtime_error("hit size mismatch");
      out.clear();lo=1536;hi=-1;tlo=1024;thi=-1;if(hist)hist->Reset("ICES");
      for(size_t i=0;i<strips->size();++i){short s=strips->at(i),t=clocks->at(i);
        if(s<0 || s>=1536 || t<0 || t>=2048) throw std::runtime_error("invalid hit");
        out.push_back({s,t});lo=std::min(lo,int(s));hi=std::max(hi,int(s));tlo=std::min(tlo,int(t));thi=std::max(thi,int(t));if(hist)hist->Fill(s,t);
      }
    };
    hits(root_as_,root_ac_,anode_hits_,Track_A,min_a,max_a,min_ad,max_ad);
    hits(root_cs_,root_cc_,cathode_hits_,Track_C,min_c,max_c,min_cd,max_cd);
    auto records=[](const auto* boards,const auto* clocks,auto& out){
      if(boards->size()!=clocks->size()) throw std::runtime_error("record size mismatch");out.clear();
      for(size_t i=0;i<boards->size();++i){short b=boards->at(i),t=clocks->at(i);if(b<0 || b>=12 || t<0 || t>=2048)throw std::runtime_error("invalid record");out.push_back({b,t});}
    };
    records(root_ab_,root_ar_,anode_records_);records(root_cb_,root_cr_,cathode_records_);
    if(root_loopback_)set_evs("IDcheck_Loopback");
    if(!N_anode.empty())set_evs("Anode_Data");
    if(!N_cathode.empty())set_evs("Cathode_Data");
    return AS_OK;
  } catch(const std::exception& e){std::cerr<<"ReadData: "<<e.what()<<std::endl;return AS_CRITICAL_ERROR_TO_FINALIZE;}
}
