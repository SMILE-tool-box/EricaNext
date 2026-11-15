#include "ReadData_2p.h"

void ReadData_2p::Scan_CP80256(){
  for(int i=0; i<36; i++){
    char f_ha[100];
    sprintf(f_ha, "HA%02d_00000.raw", i+1);
    if(FileExists(f_ha))
      N_HA2p.push_back(i+1);
  }
  if(N_HA2p.size()){
    define_evs("ClearPulse80256");
    if(exist_module("SaveFile")){
      SM2p_HA = new CP80256_2p* [N_HA2p.size()];
      for(unsigned i=0; i<N_HA2p.size(); i++){
	SM2p_HA[i] = new CP80256_2p(DataDir, N_HA2p[i]);
      }
    }
  }
}

void ReadData_2p::Read_CP80256(){
  // the ADC words are decoded on demand by Get_ADC_HA2p / Get_ADC_HA2p_Raw
}

void ReadData_2p::Get_ADC_HA2p(int ha, short *data) const {
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

void ReadData_2p::Get_ADC_HA2p_Raw(int ha, short *data, bool *err) const {
  int offset[24]
    = { 6, 22,  8,  7, 23,  9,  4, 20, 10,  5, 21, 11,
       14, 16,  0, 15, 17,  1, 12, 18,  2, 13, 19,  3};

  for(int ch=0; ch<24; ch++){
    short tmp;
    bool ng_flag = SM2p_HA[ha]->GetADC(ch, tmp);

    data[offset[ch]] = tmp;
    err[offset[ch]]  = ng_flag;
  }
}
