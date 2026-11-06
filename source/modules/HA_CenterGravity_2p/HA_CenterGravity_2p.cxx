#include "HA_CenterGravity_2p.h"

ANLStatus HA_CenterGravity_2p::mod_define(){
  define_parameter("threshold", &mod_class::threshold);
  define_parameter("overflow", &mod_class::overflow);
  
  return AS_OK;
}

ANLStatus HA_CenterGravity_2p::mod_initialize(){
  if(!(is_evs_defined("ClearPulse80256")))
    return AS_QUIT;
  get_module("ReadData_2p", &RawData);
  ha_ID = RawData->Get_HA2pID();
  N_HA = (int)(ha_ID->size());
  N_PMT = RawData->Get_Npmt_HA2p();
  N_ADC = RawData->Get_Nch_HA2p();
  
  define_evs("HA2p_overflow");
  define_evs("HA2p_data_lack");
  define_evs("HA2p_SingleHit");
  define_evs("HA2p_DoubleHit");
  define_evs("HA2p_Over3hits");
  define_evs("HA2p_no_hit");
  gr_x = new double[N_HA * N_PMT];
  gr_y = new double[N_HA * N_PMT];
  gr_e = new double[N_HA * N_PMT];
  raw_e = new double[N_HA * N_PMT];
  
  return AS_OK;
}

ANLStatus HA_CenterGravity_2p::mod_analyze(){
  std::vector<std::thread> threads;
  for(int ha=0; ha<N_HA; ha++)
    threads.push_back(std::thread(&HA_CenterGravity_2p::CG_Process, this, ha));
  for(std::thread &t: threads)
    t.join();
    
  // SMILE_tool_box HA_Center_Gravity::Process() never skips the chain here
  // (it computes HA_Hit_Flag per pixel and always continues); these evs
  // flags are diagnostics only.
  hit_ID.clear();
  for(int i=0; i<N_HA*N_PMT; i++){
    if(gr_e[i] > 0)  hit_ID.push_back(i);
  }
  switch(hit_ID.size()){
  case 0:  set_evs("HA2p_no_hit");     break;
  case 1:  set_evs("HA2p_SingleHit");  break;
  case 2:  set_evs("HA2p_DoubleHit");  break;
  default: set_evs("HA2p_Over3hits");
  }
  
  return AS_OK;
}

void HA_CenterGravity_2p::CG_Process(int id){
  short adc[24];
  bool  err[24];
  RawData->Get_ADC_HA2p_Raw(id, adc, err);

  for(int pmt=0; pmt<N_PMT; pmt++){
    double eng=0, raw=0;
    short n_flag=0;
    bool has_error=false;

    for(int ch=0; ch<N_ADC; ch++){
      short val = adc[N_ADC*pmt+ch];
      // SMILE_tool_box HA_Center_Gravity::Process(): HA_RAW_E always sums
      // the masked ADC value, independent of the per-channel error bit.
      raw += val;
      if(err[N_ADC*pmt+ch])  has_error = true;

      if(val > overflow){
	std::lock_guard<std::mutex> lock(key);
	set_evs("HA2p_overflow");
      }
      else if(val >= threshold){
	eng += val;
	n_flag++;
      }
    }
    raw_e[N_PMT*id+pmt] = raw;

    gr_e[N_PMT*id+pmt] = -1;
    if(!has_error && n_flag==4){
      gr_e[N_PMT*id+pmt] = eng;
      gr_x[N_PMT*id+pmt]
	= ((adc[N_ADC*pmt+0] + adc[N_ADC*pmt+1])
	   - (adc[N_ADC*pmt+2] + adc[N_ADC*pmt+3])) / eng;
      gr_y[N_PMT*id+pmt]
	= ((adc[N_ADC*pmt+0] + adc[N_ADC*pmt+3])
	   - (adc[N_ADC*pmt+1] + adc[N_ADC*pmt+2])) / eng;
    }
    else if(n_flag > 0 && eng > threshold){
      std::lock_guard<std::mutex> lock(key);
      set_evs("HA2p_data_lack");
    }
  }
}

void HA_CenterGravity_2p::Hit_Info(int id, int &ha, int &pmt,
				double &e, double &x, double &y)  const {
  ha = (*ha_ID)[(int)(id / N_PMT)];
  pmt = id % N_PMT;

  e = gr_e[id];
  x = gr_x[id];
  y = gr_y[id];
}

ANLStatus HA_CenterGravity_2p::mod_finalize(){
  delete [] gr_x;  delete [] gr_y;  delete [] gr_e;  delete [] raw_e;

  return AS_OK;
}

HA_CenterGravity_2p::HA_CenterGravity_2p(){
  threshold = 33;
  overflow = 4000;
}

HA_CenterGravity_2p::~HA_CenterGravity_2p(){}

