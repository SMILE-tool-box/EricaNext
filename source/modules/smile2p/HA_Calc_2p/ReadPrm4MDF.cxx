#include "HA_Calc_2p.h"

bool HA_Calc_2p::ReadPrm4MDF(){
  mdf_x = new MDF_2p* [ha_id->size()];
  mdf_y = new MDF_2p* [ha_id->size()];
  
  for(unsigned i=0; i<ha_id->size(); i++){
    // SMILE_tool_box HA_Calc_s2::Init(): XMDF_HA%02d.dat / YMDF_HA%02d.dat,
    // a flat 110-double-per-PMT TMultiDimFit dump.
    std::stringstream xs;
    xs << PrmDir << "/XMDF_HA"
       << std::setw(2) << std::setfill('0') << (*ha_id)[i]
       << ".dat";
    mdf_x[i] = new MDF_2p();
    mdf_x[i]->SetParametersFlat(xs.str().c_str(), N_PMT);

    std::stringstream ys;
    ys << PrmDir << "/YMDF_HA"
       << std::setw(2) << std::setfill('0') << (*ha_id)[i]
       << ".dat";
    mdf_y[i] = new MDF_2p();
    mdf_y[i]->SetParametersFlat(ys.str().c_str(), N_PMT);
  }
  
  return 0;
}
