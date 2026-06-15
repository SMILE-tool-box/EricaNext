#include "ReadData.h"

void ReadData::Scan_CP80256(){
  for(int i=0; i<36; i++){
    char f_ha[100];
    sprintf(f_ha, "HA%02d_00000.raw", i+1);
    if(FileExists(f_ha))
      N_HA2p.push_back(i+1);
  }
  if(N_HA2p.size()){
    define_evs("ClearPulse80256");
    if(exist_module("SaveFile")){
      SM2p_HA = new CP80256* [N_HA2p.size()];
      HA2p_OK = new TH2D* [N_HA2p.size()];
      HA2p_NG = new TH2D* [N_HA2p.size()];
      for(unsigned i=0; i<N_HA2p.size(); i++){
	SM2p_HA[i] = new CP80256(DataDir, N_HA2p[i]);
	
	char h_ha[100];
	sprintf(h_ha, "HA%02d_OK", N_HA2p[i]);
	HA2p_OK[i] = new TH2D(h_ha, h_ha, 4096, -0.5, 4095.5, 4*6, -0.5, 23.5);
	sprintf(h_ha, "HA%02d_NG", N_HA2p[i]);
	HA2p_NG[i] = new TH2D(h_ha, h_ha, 4096, -0.5, 4095.5, 4*6, -0.5, 23.5);      
	HA2p_OK[i]->SetXTitle("[ADU]");
	HA2p_OK[i]->SetYTitle("ADC Ch");
	HA2p_NG[i]->SetXTitle("[ADU]");
	HA2p_NG[i]->SetYTitle("ADC Ch");
      }
    }
  }
}

void ReadData::Read_CP80256(){
  short data;
  for(unsigned i=0; i<N_HA2p.size(); i++){
    for(int ch=0; ch<24; ch++){
      bool ha_ng_flag = SM2p_HA[i]->GetADC(ch, data);
      if(exist_module("SaveFile")){
	if(ha_ng_flag)  HA2p_NG[i]->Fill(data, ch);
	else            HA2p_OK[i]->Fill(data, ch);
      }
    }
  }
}

void ReadData::Get_ADC_HA2p(int ha, short *data) const {
  int offset[24]
    = { 6, 22,  8,  7, 23,  9,  4, 20, 10,  5, 21, 11,
       14, 16,  0, 15, 17,  1, 12, 18,  2, 13, 19,  3};
  
  for(int ch=0; ch<24; ch++){
    short tmp;
    bool ng_flag = SM2p_HA[ha]->GetADC(ch, tmp);

    if(ng_flag)  tmp = -1;
    data[(offset[ch])] = tmp;
  }
}
