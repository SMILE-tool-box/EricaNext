#include "HA_CenterGravity.h"

ANLStatus HA_CenterGravity::mod_define(){
  define_parameter("threshold", &mod_class::threshold);
  define_parameter("overflow", &mod_class::overflow);
  
  return AS_OK;
}

ANLStatus HA_CenterGravity::mod_initialize(){
  if(!(is_evs_defined("ClearPulse80256")))
    return AS_QUIT;
  get_module("ReadData", &RawData);
  ha_ID = RawData->Get_HA2pID();
  N_HA = (int)(ha_ID->size());
  N_PMT = RawData->Get_Npmt_HA2p();
  N_ADC = RawData->Get_Nch_HA2p();
  
  if(exist_module("SaveFile")){
    SaveFile* saveModule = nullptr;
    get_module_NC("SaveFile", &saveModule);
    saveModule->cd();
    saveModule->GetDirectory()->mkdir("HA_CGravity");
    saveModule->GetDirectory()->cd("HA_CGravity");
    
    GrvImg_HA2p = new TH2D* [N_HA * N_PMT];
    for(int ha=0; ha<N_HA; ha++){
      for(int pmt=0; pmt<N_PMT; pmt++){
	char hist[100];
	sprintf(hist, "GravityMap_HA%02d_PMT%d", (*ha_ID)[ha], pmt+1);
	GrvImg_HA2p[6*ha+pmt]
	  = new TH2D(hist, hist, 300, -.3, .3, 300, -.5, .5);
      }
    }
  }

  define_evs("HA2p_overflow");
  define_evs("HA2p_data_lack");
  define_evs("HA2p_SingleHit");
  define_evs("HA2p_DoubleHit");
  define_evs("HA2p_Over3hits");
  define_evs("HA2p_no_hit");
  gr_x = new double[N_HA * N_PMT];
  gr_y = new double[N_HA * N_PMT];
  gr_e = new double[N_HA * N_PMT];
  
  return AS_OK;
}

ANLStatus HA_CenterGravity::mod_analyze(){
  std::vector<std::thread> threads;
  for(int ha=0; ha<N_HA; ha++)
    threads.push_back(std::thread(&HA_CenterGravity::CG_Process, this, ha));
  for(std::thread &t: threads)
    t.join();
    
  hit_ID.clear();
  for(int i=0; i<N_HA*N_PMT; i++){
    if(gr_e[i] > 0)       hit_ID.push_back(i);
    else if(evs("HA2p_overflow") || evs("HA2p_data_lack"))
      return AS_SKIP;
  }
  if(hit_ID.size() == 0){
    set_evs("HA2p_no_hit");
    return AS_SKIP;
  }

  switch(hit_ID.size()){
  case 1:  set_evs("HA2p_SingleHit");  break;
  case 2:  set_evs("HA2p_DoubleHit");  break;
  default: set_evs("HA2p_Over3hits");
  }
  
  return AS_OK;
}

void HA_CenterGravity::CG_Process(int id){
  short adc[24];
  RawData->Get_ADC_HA2p(id, adc);

  for(int pmt=0; pmt<N_PMT; pmt++){
    double eng=0;
    short n_flag=0;
      
    for(int ch=0; ch<N_ADC; ch++){
      if(adc[N_ADC*pmt+ch] > overflow){
	std::lock_guard<std::mutex> lock(key);
	set_evs("HA2p_overflow");
      }
      else if(adc[N_ADC*pmt+ch] > threshold){
	eng += adc[N_ADC*pmt+ch];
	n_flag++;
      }
    }

    gr_e[N_PMT*id+pmt] = -1;
    if(n_flag==4){
      gr_e[N_PMT*id+pmt] = eng;
      gr_x[N_PMT*id+pmt]
	= ((adc[N_ADC*pmt+0] + adc[N_ADC*pmt+1])
	   - (adc[N_ADC*pmt+2] + adc[N_ADC*pmt+3])) / eng;
      gr_y[N_PMT*id+pmt]
	= ((adc[N_ADC*pmt+0] + adc[N_ADC*pmt+3])
	   - (adc[N_ADC*pmt+1] + adc[N_ADC*pmt+2])) / eng;
      if(exist_module("SaveFile"))
	GrvImg_HA2p[N_PMT*id+pmt]
	  ->Fill(gr_x[N_PMT*id+pmt], gr_y[N_PMT*id+pmt]);
    }
    else if(n_flag > 0 && eng > threshold){
      std::lock_guard<std::mutex> lock(key);
      set_evs("HA2p_data_lack");
    }
  }
}

void HA_CenterGravity::Hit_Info(int id, int &ha, int &pmt,
				double &e, double &x, double &y)  const {
  ha = (*ha_ID)[(int)(id / N_PMT)];
  pmt = id % N_PMT;

  e = gr_e[id];
  x = gr_x[id];
  y = gr_y[id];
}

ANLStatus HA_CenterGravity::mod_finalize(){
  delete [] gr_x;  delete [] gr_y;  delete [] gr_e;

  return AS_OK;
}

HA_CenterGravity::HA_CenterGravity(){
  threshold = 33;
  overflow = 4000;
}

HA_CenterGravity::~HA_CenterGravity(){}

