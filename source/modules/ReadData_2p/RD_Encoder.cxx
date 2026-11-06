#include "ReadData_2p.h"

void ReadData_2p::Scan_Encoder(){
  for(int i=0; i<32; i++){
    char f_anode[100];
    sprintf(f_anode, "uTPC_anode%d_00000.raw", i+1);
    // boards that were not read out leave a first file smaller than one event (TPC-only runs of 2017)
    if(FileExists(f_anode) && !FileEmpty(f_anode))
      N_anode.push_back(i+1);

    char f_cathode[100];
    sprintf(f_cathode, "uTPC_cathode%d_00000.raw", i+1);
    if(FileExists(f_cathode) && !FileEmpty(f_cathode))
      N_cathode.push_back(i+1);
  }
  if(N_anode.size()){
    define_evs("Anode_Data");
    Track_A = new TH2C("Track_A", "Track_A",
		       128*N_anode.size(), -0.5, 128*N_anode.size()-0.5,
		       1024, -0.5, 1023.5);			 
    // per-event work histogram for TPC_Calibration_2p / TOT_Skewness_2p, never written
    Track_A->SetDirectory(nullptr);
    if(exist_module("SaveFile")){
      Anode = new TPC_Encoder_2p* [N_anode.size()];
      for(unsigned i=0; i<N_anode.size(); i++)
	Anode[i] = new TPC_Encoder_2p(DataDir, 0, N_anode[i], Reverse);
    }
  }
  if(N_cathode.size()){
    define_evs("Cathode_Data");
    Track_C = new TH2C("Track_C", "Track_C",
		       128*N_anode.size(), -0.5, 128*N_anode.size()-0.5,
		       1024, -0.5, 1023.5);			 
    // per-event work histogram for TPC_Calibration_2p / TOT_Skewness_2p, never written
    Track_C->SetDirectory(nullptr);
    if(exist_module("SaveFile")){
      Cathode = new TPC_Encoder_2p* [N_cathode.size()];
      for(unsigned i=0; i<N_cathode.size(); i++)
	Cathode[i] = new TPC_Encoder_2p(DataDir, 1, N_cathode[i], Reverse);
    }
  }
}

void ReadData_2p::Read_Anode(){
  Track_A->Reset("ICES");
  n_hit_a_ = 0;
  hit_strip_a_.clear();  hit_clk_a_.clear();
  rec_board_a_.clear();  rec_clk_a_.clear();
  min_a = 128 * N_anode.size();  max_a = -1;
  min_ad = 1024;  max_ad = -1;
  
  for(unsigned i=0; i<N_anode.size(); i++){
    if(exist_module("SaveFile")){
      if(Anode[i]->TrackSize()){
	for(unsigned j=0; j<Anode[i]->TrackSize(); j+=5){
	  int clk = (Anode[i]->TrackData(j)) & 0x7ff;
	  rec_board_a_.push_back((short)i);
	  rec_clk_a_.push_back((short)clk);
	  for(int ch=0; ch<4; ch++){
	    for(int bit=0; bit<32; bit++){
	      char str_data;
	      if(Reverse) str_data = ((Anode[i]->TrackData(j+ch+1)) >> (31-bit)) & 0x1;
	      else        str_data = ((Anode[i]->TrackData(j+ch+1)) >> bit) & 0x1;
	      if(str_data){
		int n_strip = (int)i*128+32*ch+bit;
		Track_A->Fill(n_strip, clk);
		n_hit_a_++;
		hit_strip_a_.push_back((short)n_strip);
		hit_clk_a_.push_back((short)clk);
		if(min_a>n_strip)  min_a = n_strip;
		if(max_a<n_strip)  max_a = n_strip;
		if(min_ad>clk)     min_ad= clk;
		if(max_ad<clk)     max_ad= clk;
	      }
	    }
	  }
	}
      }
    }
  }
}

void ReadData_2p::Read_Cathode(){
  Track_C->Reset("ICES");
  n_hit_c_ = 0;
  hit_strip_c_.clear();  hit_clk_c_.clear();
  rec_board_c_.clear();  rec_clk_c_.clear();
  min_c = 128 * N_cathode.size();  max_c = -1;
  min_cd = 1024;  max_cd = -1;
  
  for(unsigned i=0; i<N_cathode.size(); i++){
    if(exist_module("SaveFile")){
      if(Cathode[i]->TrackSize()){
	for(unsigned j=0; j<Cathode[i]->TrackSize(); j+=5){
	  int clk = (Cathode[i]->TrackData(j)) & 0x7ff;
	  rec_board_c_.push_back((short)i);
	  rec_clk_c_.push_back((short)clk);
	  for(int ch=0; ch<4; ch++){
	    for(int bit=0; bit<32; bit++){
	      char str_data;
	      if(Reverse) str_data = ((Cathode[i]->TrackData(j+ch+1)) >> (31-bit)) & 0x1;
	      else        str_data = ((Cathode[i]->TrackData(j+ch+1)) >> bit) & 0x1;
	      if(str_data){
		int n_strip = (int)i*128+32*ch+bit;
		Track_C->Fill(n_strip, clk);
		n_hit_c_++;
		hit_strip_c_.push_back((short)n_strip);
		hit_clk_c_.push_back((short)clk);
		if(min_c>n_strip)  min_c = n_strip;
		if(max_c<n_strip)  max_c = n_strip;
		if(min_cd>clk)     min_cd= clk;
		if(max_cd<clk)     max_cd= clk;
	      }
	    }
	  }
	}
      }
    }
  }
}

void ReadData_2p::Get_TPC_FADC(bool c_flag, int id, short *data) const {
  for(int ch=0; ch<4; ch++)
    for(int clk=0; clk<512; clk++){
      if(c_flag){
	if(Cathode[id]->TrackSize())
	  data[ch*512+clk] = 1023 - Cathode[id]->FADC_data(ch, clk);
	else
	  data[ch*512+clk] = 0;
      }	  
      else{
	if(Anode[id]->TrackSize())
	  data[ch*512+clk] = Anode[id]->FADC_data(ch, clk);
	else
	  data[ch*512+clk] = 0;
      }
    }
}
