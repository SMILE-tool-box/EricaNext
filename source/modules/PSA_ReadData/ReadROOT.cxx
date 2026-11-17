#include "PSA_ReadData.h"
#include "ReadData.h"
#include <iostream>
#include <set>
#include <stdexcept>
ANLStatus PSA_ReadData::InitializeROOT(){
 try {
  root_input_.reset(TFile::Open(root_file_.c_str(),"READ"));
  if(!root_input_ || root_input_->IsZombie()) throw std::runtime_error("cannot open ROOT input");
  auto* info=dynamic_cast<TTree*>(root_input_->Get("PSA_RawTreeWriter/run_info"));
  root_tree_=dynamic_cast<TTree*>(root_input_->Get("PSA_RawTreeWriter/raw"));
  if(!info || !root_tree_ || info->GetEntries()!=1) throw std::runtime_error("missing PSA raw/run_info (processed psa tree cannot replace raw waveforms)");
  int version=0, ns=0, nsignal=0;
  if(info->SetBranchAddress("schema_version",&version)<0 || info->SetBranchAddress("n_sample",&ns)<0 || info->SetBranchAddress("n_sig",&nsignal)<0)
    throw std::runtime_error("missing PSA schema metadata");
  std::vector<int>* ids=nullptr;
  if(info->SetBranchAddress("board_ids",&ids)<0 || info->GetEntry(0)<=0 || !ids) throw std::runtime_error("invalid board_ids");
  if(version!=1 || ns!=50 || nsignal!=8)throw std::runtime_error("unsupported PSA raw schema");
  board_ids_=*ids;info->ResetBranchAddresses();delete ids;
  std::set<int> seen;
  for(int id:board_ids_){if(id<0 || id>=100 || !seen.insert(id).second)throw std::runtime_error("invalid board ID");boards_.push_back(new PSA_Board(id,{}));}
  if(boards_.empty())throw std::runtime_error("no PSA boards");
  auto bind=[&](const char* name,auto* addr){if(root_tree_->SetBranchAddress(name,addr)<0)throw std::runtime_error(std::string("invalid branch: ")+name);};
  bind("event_id",&root_trig_);bind("sevt_id",&root_sevt_);bind("epoch",&root_epoch_);
  bind("board",&root_board_);bind("board_epoch",&root_board_epoch_);bind("clk",&root_clk_);
  bind("hit_mask",&root_mask_);bind("time",&root_time_);bind("temp_raw",&root_temp_);bind("adc",&root_adc_);
  if(root_first_entry_<0 || root_first_entry_>=root_tree_->GetEntries()) throw std::runtime_error("root_first_entry out of range");
  root_entry_=root_first_entry_;
  return AS_OK;
 }catch(const std::exception& e){std::cerr<<"PSA_ReadData: "<<root_file_<<": "<<e.what()<<std::endl;return AS_CRITICAL_ERROR_TO_FINALIZE;}
}
ANLStatus PSA_ReadData::ReadROOT(){
 try {
  // TPC rows skipped by ReadData (unreadable) did not reach this module: follow its row in the same raw ROOT
  if(sync_ && tpc_->Get_RootRow()>=0) root_entry_=tpc_->Get_RootRow();
  if(root_entry_>=root_tree_->GetEntries()){
    if(sync_)throw std::runtime_error("PSA raw ended before TPC");
    return AS_QUIT_ALL;
  }
  if(root_tree_->GetEntry(root_entry_++)<=0){
    if(!root_skip_unreadable_)throw std::runtime_error("cannot read event");
    // e.g. a basket of the file filled with zeros (pages lost when the file was written to the NAS)
    if(++root_unreadable_<=20)
      std::cerr<<"PSA_ReadData: WARNING: row "<<root_entry_-1<<" of "<<root_file_<<" cannot be read; skipped"
               <<(root_unreadable_==20 ? " (further rows: count only)" : "")<<std::endl;
    present_.assign(boards_.size(),false);n_present_=0;
    return AS_SKIP;
  }
  if(sync_ && (root_trig_!=tpc_->Get_EventID_coincidence() || root_sevt_!=tpc_->Get_SEvtID()))throw std::runtime_error("TPC/PSA rows are not synchronized; use the same combined raw ROOT");
  const size_t n=root_board_->size();
  if(root_board_epoch_->size()!=n || root_clk_->size()!=n || root_mask_->size()!=n || root_time_->size()!=n || root_temp_->size()!=n*6 || root_adc_->size()!=n*2400)throw std::runtime_error("PSA vector size mismatch");
  present_.assign(boards_.size(),false);n_present_=0;
  for(size_t j=0;j<n;++j){
    int i=Index(root_board_->at(j));if(i<0 || present_[i] || root_mask_->at(j)>63)throw std::runtime_error("invalid/duplicate PSA board or mask");
    for(size_t k=j*2400;k<(j+1)*2400;++k)if(root_adc_->at(k)>16383)throw std::runtime_error("invalid PSA ADC");
    boards_[i]->Restore(root_trig_,root_clk_->at(j),root_board_epoch_->at(j),root_time_->at(j),root_mask_->at(j),root_temp_->data()+j*6,root_adc_->data()+j*2400);
    present_[i]=true;++n_present_;
    if(hit_pattern_)for(int ch=0;ch<6;++ch)if(boards_[i]->Hit(ch))hit_pattern_->Fill(boards_[i]->ID(),ch);
  }
  trig_=root_trig_;sevt_id_=root_sevt_;epoch_=root_epoch_;
  return AS_OK;
 }catch(const std::exception& e){std::cerr<<"PSA_ReadData: "<<e.what()<<std::endl;return AS_CRITICAL_ERROR_TO_FINALIZE;}
}
