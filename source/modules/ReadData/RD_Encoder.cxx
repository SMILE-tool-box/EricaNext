#include "ReadData.h"

void ReadData::Scan_Encoder(){
  for(int i=0; i<32; i++){
    char filename[100];
    sprintf(filename, "tpc_enc_a%02d_00000.raw", i);
    std::string dir = FindDir(filename);
    if(!dir.empty()){
      N_anode.push_back(i);
      DirAnode.push_back(dir);
    }
    sprintf(filename, "tpc_enc_c%02d_00000.raw", i);
    dir = FindDir(filename);
    if(!dir.empty()){
      N_cathode.push_back(i);
      DirCathode.push_back(dir);
    }
  }
  if(N_anode.size()){
    define_evs("Anode_Data");
    Anode = new TPC_Encoder* [N_anode.size()];
    for(unsigned i=0; i<N_anode.size(); i++)
      Anode[i] = new TPC_Encoder(DirAnode[i], 0, N_anode[i], Reverse, UseNtime);
  }
  if(N_cathode.size()){
    define_evs("Cathode_Data");
    Cathode = new TPC_Encoder* [N_cathode.size()];
    for(unsigned i=0; i<N_cathode.size(); i++)
      Cathode[i] = new TPC_Encoder(DirCathode[i], 1, N_cathode[i], Reverse, UseNtime);
  }
}

void ReadData::Read_Anode(){
  if(Track_A) Track_A->Reset("ICES");
  anode_hits_.clear();
  anode_records_.clear();
  min_a = 128*12; max_a = -1;
  min_ad = 1024; max_ad = -1;
  for(unsigned i=0; i<N_anode.size(); i++){
    for(unsigned j=0; j<Anode[i]->TrackSize(); j+=5){
      int clk = Anode[i]->TrackData(j) & 0x7ff;
      anode_records_.push_back({(short)N_anode[i], (short)clk});
      for(int ch=0; ch<4; ch++){
        for(int bit=0; bit<32; bit++){
          int shift = Reverse ? 31-bit : bit;
          if((Anode[i]->TrackData(j+ch+1) >> shift) & 0x1){
            int strip = N_anode[i]*128+32*ch+bit;
            anode_hits_.push_back({(short)strip, (short)clk});
            if(Track_A) Track_A->Fill(strip, clk);
            if(min_a>strip) min_a = strip;
            if(max_a<strip) max_a = strip;
            if(min_ad>clk) min_ad = clk;
            if(max_ad<clk) max_ad = clk;
          }
        }
      }
    }
  }
}

void ReadData::Read_Cathode(){
  if(Track_C) Track_C->Reset("ICES");
  cathode_hits_.clear();
  cathode_records_.clear();
  min_c = 128*12; max_c = -1;
  min_cd = 1024; max_cd = -1;
  for(unsigned i=0; i<N_cathode.size(); i++){
    for(unsigned j=0; j<Cathode[i]->TrackSize(); j+=5){
      int clk = Cathode[i]->TrackData(j) & 0x7ff;
      cathode_records_.push_back({(short)N_cathode[i], (short)clk});
      for(int ch=0; ch<4; ch++){
        for(int bit=0; bit<32; bit++){
          int shift = Reverse ? 31-bit : bit;
          if((Cathode[i]->TrackData(j+ch+1) >> shift) & 0x1){
            int strip = N_cathode[i]*128+32*ch+bit;
            cathode_hits_.push_back({(short)strip, (short)clk});
            if(Track_C) Track_C->Fill(strip, clk);
            if(min_c>strip) min_c = strip;
            if(max_c<strip) max_c = strip;
            if(min_cd>clk) min_cd = clk;
            if(max_cd<clk) max_cd = clk;
          }
        }
      }
    }
  }
}

const TH2C* ReadData::Get_Track_A() const {
  if(!Track_A && !N_anode.empty()){
    Track_A = new TH2C("Track_A", "Track_A",
                           128*12, -0.5, 128*12-0.5, 1024, -0.5, 1023.5);
    Track_A->SetDirectory(nullptr);
    for(const auto& hit : anode_hits_) Track_A->Fill(hit.strip, hit.clock);
  }
  return Track_A;
}

const TH2C* ReadData::Get_Track_C() const {
  if(!Track_C && !N_cathode.empty()){
    Track_C = new TH2C("Track_C", "Track_C",
                           128*12, -0.5, 128*12-0.5, 1024, -0.5, 1023.5);
    Track_C->SetDirectory(nullptr);
    for(const auto& hit : cathode_hits_) Track_C->Fill(hit.strip, hit.clock);
  }
  return Track_C;
}

void ReadData::Get_TPC_FADC(bool c_flag, int id, short *data) const {
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
