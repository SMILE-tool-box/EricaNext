#include "CalcFADC.h"

// FADC waveform -> charge of every anode and cathode channel (sum of the samples more than `threshold`
// above the baseline = mean of the first `base_window` samples). The charges go to TPC_Calibration.
// This module makes no histogram and sets no flag: the 1-hit spectra etc. are made afterwards from the
// raw data tree (smile3_raw / RawTreeWriter_sm3, scripts/calib/make_tpc_spec1hit.py).
// Every event goes on to the next modules (as SMILE_tool_box CalcFADC::Process()).

ANLStatus CalcFADC::mod_define(){
  define_parameter("base_window", &mod_class::base_window);
  define_parameter("threshold", &mod_class::threshold);

  return AS_OK;
}

ANLStatus CalcFADC::mod_initialize(){
  if(!(exist_module("ReadData")))
    return AS_QUIT;
  get_module("ReadData", &RawData);
  Q_anode   = new double [4*12];
  Q_cathode = new double [4*12];

  return AS_OK;
}

ANLStatus CalcFADC::mod_analyze(){
  unsigned N_a = RawData->Get_AnodeID()->size();
  unsigned N_c = RawData->Get_CathodeID()->size();

  std::fill(Q_anode,   Q_anode   + 4*12, 0.0);
  std::fill(Q_cathode, Q_cathode + 4*12, 0.0);

  std::vector<std::thread> threads;
  threads.push_back(std::thread(&CalcFADC::IntegWave, this, 0, N_a));
  threads.push_back(std::thread(&CalcFADC::IntegWave, this, 1, N_c));
  for(std::thread &t: threads)
    t.join();

  return AS_OK;
}

void CalcFADC::IntegWave(bool c_flag, unsigned Nid){
  int Nclk = (RawData->Get_FADC_clk());
  short *wave = new short [4*Nclk];

  for(unsigned id=0; id<Nid; id++){
    RawData->Get_TPC_FADC(c_flag, id, wave);

    for(int ch=0; ch<4; ch++){
      double mean=0, charge=0;

      for(int clk=0; clk<base_window; clk++)
	mean += wave[ch*Nclk+clk]/base_window;
      for(int clk=TMath::FloorNint(base_window); clk<Nclk; clk++){
	if(wave[ch*Nclk+clk] - mean > threshold){
	  charge += wave[ch*Nclk+clk] - mean;
	}
      }

      if(c_flag){
        int phys_c = (*RawData->Get_CathodeID())[id];
	Q_cathode[4*phys_c+ch] = charge;
      }
      else{
        int phys_a = (*RawData->Get_AnodeID())[id];
	Q_anode[4*phys_a+ch] = charge;
      }
    }
  }

  delete [] wave;
}

ANLStatus CalcFADC::mod_finalize(){
  delete [] Q_anode;  delete [] Q_cathode;

  return AS_OK;
}


CalcFADC::CalcFADC(){
  base_window = 30;
  threshold = 20;
}

CalcFADC::~CalcFADC(){
}
