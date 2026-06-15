#include "HA_Calc.h"

bool HA_Calc::ReadPrm4MDF(){
  mdf_x = new MDF* [ha_id->size()];
  mdf_y = new MDF* [ha_id->size()];
  if(exist_module("SaveFile")){
    MDF_Image = new TH2D* [N_PMT * (ha_id->size())];
  }
  
  for(unsigned i=0; i<ha_id->size(); i++){
    std::stringstream xs;
    xs << PrmDir << "/MDF_HA"
       << std::setw(2) << std::setfill('0') << (*ha_id)[i]
       << "_X.dat";
    mdf_x[i] = new MDF(xs.str().c_str(), N_PMT);

    std::stringstream ys;
    ys << PrmDir << "/MDF_HA"
       << std::setw(2) << std::setfill('0') << (*ha_id)[i]
       << "_Y.dat";
    mdf_y[i] = new MDF(ys.str().c_str(), N_PMT);

    if(exist_module("SaveFile")){
      char histname[100];
      for(int pi=0; pi<N_PMT; pi++){
	sprintf(histname, "MDF_HA%02d_PMT%d", (*ha_id)[i], pi+1);
	MDF_Image[N_PMT*i+pi]
	  = new TH2D(histname, histname, 300, -0.5, 0.5, 300, -0.5, 0.5);
      }
    }
  }
  
  return 0;
}
