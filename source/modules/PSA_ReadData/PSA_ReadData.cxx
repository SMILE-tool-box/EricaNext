#include "PSA_ReadData.h"
#include "ReadData.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <regex>

ANLStatus PSA_ReadData::mod_define(){
  define_parameter("data_dir",         &mod_class::data_dir_);
  define_parameter("extra_data_dirs",  &mod_class::extra_data_dirs_);
  define_parameter("sync_tpc",         &mod_class::sync_tpc_);
  define_parameter("save_hit_pattern", &mod_class::save_hit_pattern_);
  define_parameter("recursive",        &mod_class::recursive_);
  return AS_OK;
}

// ha<NN>_<number>.dat (or .dat.tmp) of a directory (and its subdirectories when recursive), added to found_.
// The files of one board may be spread over several directories (other storage node, part0, part1, ... of a
// long run); they are merged by number. A number found twice: the .dat rather than a .dat.tmp, else the
// first one, with a warning.
void PSA_ReadData::ScanDir(const std::string& dir){
  namespace fs = std::filesystem;
  if(!fs::is_directory(dir)){
    std::cerr << "PSA_ReadData: no directory " << dir << std::endl;
    return;
  }
  static const std::regex re("ha(\\d{2})_(\\d{5})\\.dat(\\.tmp)?");
  auto add = [&](const fs::directory_entry& e){
    std::smatch m;
    std::string name = e.path().filename().string();
    if(!std::regex_match(name, m, re))  return;
    int board = std::stoi(m[1]), number = std::stoi(m[2]);
    bool tmp = m[3].matched;
    auto& slot = found_[board];
    auto it = slot.find(number);
    if(it == slot.end())
      slot[number] = {e.path().string(), tmp};
    else if(it->second.second && !tmp)
      it->second = {e.path().string(), tmp};
    else if(it->second.second == tmp)
      std::cerr << "PSA_ReadData: WARNING: " << e.path().string() << " has the same number as "
                << it->second.first << "; the latter is used" << std::endl;
  };
  if(recursive_)
    for(const auto& e : fs::recursive_directory_iterator(dir))  add(e);
  else
    for(const auto& e : fs::directory_iterator(dir))  add(e);
}

// found_ -> files_ (per board, sorted by number)
void PSA_ReadData::CollectFiles(){
  for(auto& [board, numbers] : found_){
    if((int)files_.size() <= board)  files_.resize(board + 1);
    for(auto& [number, f] : numbers){
      files_[board].push_back(f.first);
      if(f.second){
        n_tmp_files_++;
        std::cerr << "PSA_ReadData: WARNING: using " << f.first << " (no .dat)" << std::endl;
      }
    }
  }
}

int PSA_ReadData::Index(int id) const {
  for(int i=0; i<(int)boards_.size(); i++)
    if(boards_[i]->ID() == id)  return i;
  return -1;
}

ANLStatus PSA_ReadData::mod_initialize(){
  ScanDir(data_dir_);
  for(const auto& d : extra_data_dirs_)  ScanDir(d);
  CollectFiles();
  for(int id=0; id<(int)files_.size(); id++){
    if(files_[id].empty())  continue;
    boards_.push_back(new PSA_Board(id, files_[id]));
    board_ids_.push_back(id);
  }
  if(boards_.empty()){
    std::cerr << "PSA_ReadData: no ha<NN>_<number>.dat in " << data_dir_ << std::endl;
    return AS_QUIT;
  }
  std::cout << "PSA_ReadData: " << boards_.size() << " boards:";
  for(int id : board_ids_)  std::cout << " " << PSA_Board::Name(id);
  std::cout << std::endl;

  present_.assign(boards_.size(), false);
  consumed_.assign(boards_.size(), true);   // read the first frame at the first event

  if(sync_tpc_ && exist_module("ReadData")){
    get_module("ReadData", &tpc_);
    sync_ = true;
    std::cout << "PSA_ReadData: following the events of ReadData (its trigger counter)" << std::endl;
  }

  if(exist_module("SaveFile") && save_hit_pattern_){
    SaveFile* saveModule = nullptr;
    get_module_NC("SaveFile", &saveModule);
    saveModule->cd();
    saveModule->GetDirectory()->mkdir("PSA_ReadData");
    saveModule->GetDirectory()->cd("PSA_ReadData");
    int nb = board_ids_.back() + 1;
    hit_pattern_ = new TH2D("HitPattern", "PSA hits;HA board;ch", nb, -0.5, nb - 0.5,
                            PSA_Board::N_CH, -0.5, PSA_Board::N_CH - 0.5);
  }
  return AS_OK;
}

ANLStatus PSA_ReadData::mod_analyze(){
  const int nb = (int)boards_.size();
  for(int i=0; i<nb; i++){
    if(consumed_[i])  boards_[i]->Next();
    consumed_[i] = false;
    present_[i] = false;
  }

  if(sync_){
    // follow ReadData: skip frames of earlier triggers, keep frames of later ones
    unsigned id = tpc_->Get_EventID_coincidence();
    for(int i=0; i<nb; i++){
      PSA_Board* b = boards_[i];
      while(!b->End() && b->Trig() < id)  b->Next();
    }
    trig_ = id;
    epoch_ = 0;
  }
  else{
    // smallest (epoch, trig) of the boards
    bool any = false;
    for(int i=0; i<nb; i++){
      PSA_Board* b = boards_[i];
      if(b->End())  continue;
      if(!any || b->Epoch() < epoch_ || (b->Epoch() == epoch_ && b->Trig() < trig_)){
        epoch_ = b->Epoch();
        trig_  = b->Trig();
      }
      any = true;
    }
    if(!any)  return AS_QUIT_ALL;
  }

  n_present_ = 0;
  for(int i=0; i<nb; i++){
    PSA_Board* b = boards_[i];
    if(b->End() || b->Trig() != trig_ || (!sync_ && b->Epoch() != epoch_))  continue;
    present_[i] = consumed_[i] = true;
    n_present_++;
    if(hit_pattern_)
      for(int ch=0; ch<PSA_Board::N_CH; ch++)
        if(b->Hit(ch))  hit_pattern_->Fill(b->ID(), ch);
  }
  sevt_id_ = next_sevt_id_++;
  return AS_OK;
}

ANLStatus PSA_ReadData::mod_finalize(){
  for(auto* b : boards_){
    std::cout << "PSA_ReadData: " << PSA_Board::Name(b->ID()) << ": " << b->NFrames() << " frames";
    if(b->NSkipped())  std::cout << ", " << b->NSkipped() << " bytes skipped";
    if(b->NResets())   std::cout << ", trig went back " << b->NResets() << " times";
    std::cout << std::endl;
    delete b;
  }
  boards_.clear();
  if(n_tmp_files_)
    std::cout << "PSA_ReadData: " << n_tmp_files_ << " .dat.tmp files were read" << std::endl;
  return AS_OK;
}

PSA_ReadData::PSA_ReadData(){
  data_dir_ = "./";
  sync_tpc_ = true;
  save_hit_pattern_ = true;
  recursive_ = false;
}

PSA_ReadData::~PSA_ReadData(){}
