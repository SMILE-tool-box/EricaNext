#include "ReadData.h"

void ReadData::Scan_Encoder(){
  for(int i=0; i<32; i++){
    char f_anode[100];
    //sprintf(f_anode, "uTPC_anode%d_00000.raw", i);
	sprintf(f_anode, "tpc_enc_a%02d_00000.raw", i);
    if(FileExists(f_anode))
      N_anode.push_back(i);

    char f_cathode[100];
    sprintf(f_cathode, "tpc_enc_c%02d_00000.raw", i);
    if(FileExists(f_cathode))
      N_cathode.push_back(i);
  }
  if(N_anode.size()){
    define_evs("Anode_Data");
    Track_A = new TH2C("Track_A", "Track_A",
		       128*N_anode.size(), -0.5, 128*N_anode.size()-0.5,
		       1024, -0.5, 1023.5);			 
    if(exist_module("SaveFile")){
		if(SaveFADC_A_filter){
        FADC_A_filter_graph = new TGraph();
        FADC_A_filter_graph->SetName("FADC_A_filter_Ntime_vs_Nch");
        FADC_A_filter_graph->SetTitle("FADC_A filter;N_{time};N_{ch} fired");
		FADC_A_filter_graph->SetMarkerStyle(20);
        FADC_A_filter_graph->SetDrawOption("AP");
		gDirectory->Append(FADC_A_filter_graph);
      }
      Anode = new TPC_Encoder* [N_anode.size()];
	  if(Image_A_C){
		Image_A = new TH2D("Image_A", "Image_A",
			 128*N_anode.size(), -0.5, 128*N_anode.size()-0.5,
			 1024, -0.5, 1023.5);
		}
      if(FADC_TH){
        FADC_Wave_A = new TH2D* [4 * N_anode.size()];
	    FADC_ev_A   = new TH2D* [4 * N_anode.size()];
	  }
      for(unsigned i=0; i<N_anode.size(); i++){
	Anode[i] = new TPC_Encoder(DataDir, 0, N_anode[i], Reverse, UseNtime);
	
	if(FADC_TH){
	char h_anode[100];
	for(int ch=0; ch<4; ch++){
	  sprintf(h_anode, "FADC_Anode%d_Ch%d", i, ch);
	  FADC_Wave_A[4*i+ch] = new TH2D(h_anode, h_anode, 512, -0.5, 511.5, 1024, -0.5, 1023.5);
	  sprintf(h_anode, "FADC_ev_Anode%d_Ch%d", i, ch);
	  FADC_ev_A[4*i+ch] = new TH2D(h_anode, h_anode,
					512, -0.5, 511.5, 1024, -0.5, 1023.5);
		}
	}
      }
    }
  }
  if(N_cathode.size()){
    define_evs("Cathode_Data");
    Track_C = new TH2C("Track_C", "Track_C",
		       128*N_cathode.size(), -0.5, 128*N_cathode.size()-0.5,
		       1024, -0.5, 1023.5);			 
    if(exist_module("SaveFile")){
		if(SaveFADC_C_filter){
        FADC_C_filter_graph = new TGraph();
        FADC_C_filter_graph->SetName("FADC_C_filter_Ntime_vs_Nch");
        FADC_C_filter_graph->SetTitle("FADC_C filter;N_{time};N_{ch} fired");
		FADC_C_filter_graph->SetMarkerStyle(20);
        FADC_C_filter_graph->SetDrawOption("AP");
		gDirectory->Append(FADC_C_filter_graph);
      }
      Cathode = new TPC_Encoder* [N_cathode.size()];
	  if(Image_A_C){
      	Image_C = new TH2D("Image_C", "Image_C",
			 128*N_cathode.size(), -0.5, 128*N_cathode.size()-0.5,
			 1024, -0.5, 1023.5);
      }
	  if(FADC_TH){
      	FADC_Wave_C = new TH2D* [4 * N_cathode.size()];
	  	FADC_ev_C   = new TH2D* [4 * N_cathode.size()];
	  }
	
      for(unsigned i=0; i<N_cathode.size(); i++){
	Cathode[i] = new TPC_Encoder(DataDir, 1, N_cathode[i], Reverse, UseNtime);
	
	if(FADC_TH){
	char h_cathode[100];
	for(int ch=0; ch<4; ch++){
	  sprintf(h_cathode, "FADC_Cathode%d_Ch%d", i, ch);
	  FADC_Wave_C[4*i+ch] = new TH2D(h_cathode, h_cathode,
					 512, -0.5, 511.5, 1024, -0.5, 1023.5);
	  sprintf(h_cathode, "FADC_ev_Cathode%d_Ch%d", i, ch);
	  FADC_ev_C[4*i+ch] = new TH2D(h_cathode, h_cathode,
					512, -0.5, 511.5, 1024, -0.5, 1023.5);
	}
	}
      }
    }
  }
}

void ReadData::Read_Anode(){
  Track_A->Reset("ICES");
  anode_hits_.clear();
  min_a = 128 * N_anode.size();  max_a = -1;
  min_ad = 1024;  max_ad = -1;
  
  for(unsigned i=0; i<N_anode.size(); i++){
    if(exist_module("SaveFile")){
		if(FADC_TH){
	  for(int ch=0; ch<4; ch++)
	FADC_ev_A[i*4+ch]->Reset("ICES");
		}
      if(Anode[i]->TrackSize()){
	for(int clk=0; clk<512; clk++)
	  for(int ch=0; ch<4; ch++){
		if(FADC_TH){
	   	 	FADC_Wave_A[i*4+ch]->Fill(clk, Anode[i]->FADC_data(ch, clk));
	    	FADC_ev_A[i*4+ch]->Fill(clk, Anode[i]->FADC_data(ch, clk));
		}
	  }
	
	for(unsigned j=0; j<Anode[i]->TrackSize(); j+=5){
	  int clk = (Anode[i]->TrackData(j)) & 0x7ff;
	  for(int ch=0; ch<4; ch++){
	    for(int bit=0; bit<32; bit++){
	      char str_data;
		 if(Reverse){
	      str_data = ((Anode[i]->TrackData(j+ch+1)) >> (31-bit)) & 0x1;
		 }
		 else{
	      str_data = ((Anode[i]->TrackData(j+ch+1)) >> bit) & 0x1;
		 }
	      if(str_data){
		int n_strip = (int)i*128+32*ch+bit;
		anode_hits_.push_back({(short)n_strip, (short)clk});
		if(Image_A_C)
		Image_A->Fill(n_strip, clk);
		Track_A->Fill(n_strip, clk);
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

void ReadData::Read_Cathode(){
  Track_C->Reset("ICES");
  cathode_hits_.clear();
  min_c = 128 * N_cathode.size();  max_c = -1;
  min_cd = 1024;  max_cd = -1;
  
  for(unsigned i=0; i<N_cathode.size(); i++){
    if(exist_module("SaveFile")){
		if(FADC_TH){
	  for(int ch=0; ch<4; ch++)
	FADC_ev_C[i*4+ch]->Reset("ICES");
		}
      if(Cathode[i]->TrackSize()){
		if(FADC_TH){
	for(int clk=0; clk<512; clk++)
	  for(int ch=0; ch<4; ch++){
	    FADC_Wave_C[i*4+ch]->Fill(clk, Cathode[i]->FADC_data(ch, clk));
	    FADC_ev_C[i*4+ch]->Fill(clk, Cathode[i]->FADC_data(ch, clk));
	  }
	}
	for(unsigned j=0; j<Cathode[i]->TrackSize(); j+=5){
	  int clk = (Cathode[i]->TrackData(j)) & 0x7ff;
	  for(int ch=0; ch<4; ch++){
	    for(int bit=0; bit<32; bit++){
	      char str_data;
		 if(Reverse){
	      str_data = ((Cathode[i]->TrackData(j+ch+1)) >> (31-bit)) & 0x1;
		 }
		 else{
	      str_data = ((Cathode[i]->TrackData(j+ch+1)) >> bit) & 0x1;
		 }
	      if(str_data){
		int n_strip = (int)i*128+32*ch+bit;
		cathode_hits_.push_back({(short)n_strip, (short)clk});
		if(Image_C)
		  Image_C->Fill(n_strip, clk);
		Track_C->Fill(n_strip, clk);
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
