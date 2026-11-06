#include "HA_Calc_2p.h"

bool HA_Calc_2p::ReadCalPrm(){
  {
    cal_a = new double [64* N_PMT * (ha_id->size())];
    cal_b = new double [64* N_PMT * (ha_id->size())];
    cal_c = new double [64* N_PMT * (ha_id->size())];

    pos_x = new double [(ha_id->size())];
    pos_y = new double [(ha_id->size())];
    pos_z = new double [(ha_id->size())];
    dir_xx = new double [(ha_id->size())];
    dir_xy = new double [(ha_id->size())];
    dir_yx = new double [(ha_id->size())];
    dir_yy = new double [(ha_id->size())];
    dir_zx = new double [(ha_id->size())];
    dir_zy = new double [(ha_id->size())];
    std::stringstream pos_s;
    pos_s << PrmDir << "/HAs.pos";
    ifstream pos_data(pos_s.str().c_str());
    
    for(unsigned i=0; i<ha_id->size(); i++){
      std::stringstream cals;
      cals << PrmDir << "/Cal_HA"
	   << std::setw(2) << std::setfill('0') << (*ha_id)[i]
	   << ".dat";
      ifstream cal_data(cals.str().c_str());

      for(int pmt=0; pmt<6; pmt++){
	for(int yj=0; yj<8; yj++)
	  for(int xi=0; xi<8; xi++){
	    int xx, yy;
	    cal_data >> xx >> yy >> cal_a[64*(N_PMT*i+pmt)+8*xi+yj];
	  }
	for(int yj=0; yj<8; yj++)
	  for(int xi=0; xi<8; xi++){
	    int xx, yy;
	    cal_data >> xx >> yy >> cal_b[64*(N_PMT*i+pmt)+8*xi+yj];
	  }
      }

      // SMILE_tool_box HA_Calc_s2::Init(): cal_c = (662 - cal_b) / cal_a,
      // i.e. the raw-ADC position of the 662 keV peak predicted by cal_a/b.
      for(int pmt=0; pmt<6; pmt++)
	for(int yj=0; yj<8; yj++)
	  for(int xi=0; xi<8; xi++){
	    unsigned idx = 64*(N_PMT*i+pmt)+8*xi+yj;
	    cal_c[idx] = (662. - cal_b[idx]) / cal_a[idx];
	  }

      // SMILE_tool_box: when Flag_Modify_PMT_Gain is set, cal_c is instead
      // read directly from the measured 137Cs 662 keV peak position
      // (3rd column of each quad), overriding the cal_a/cal_b-derived value.
      if(modify_pmt_gain){
	std::stringstream ms;
	ms << ModifiedPrmDir << "/Mean_Sigma_HA"
	   << std::setw(2) << std::setfill('0') << (*ha_id)[i]
	   << "_137Cs_662keV.dat";
	ifstream ms_data(ms.str().c_str());
	for(int pmt=0; pmt<6; pmt++)
	  for(int yj=0; yj<8; yj++)
	    for(int xi=0; xi<8; xi++){
	      double tx, ty, sigma;
	      ms_data >> tx >> ty >> cal_c[64*(N_PMT*i+pmt)+8*xi+yj] >> sigma;
	    }
      }

      int h_id;
      pos_data >> h_id;
      if(h_id == (*ha_id)[i]){
	pos_data >> pos_x[i] >> pos_y[i] >> pos_z[i]
		 >> dir_xx[i] >> dir_xy[i] >> dir_yx[i]
		 >> dir_yy[i] >> dir_zx[i] >> dir_zy[i];
      }
      else{
	for(int ti=0; ti<9; ti++){
	  double tmp;
	  pos_data >> tmp;
	}
      }
    }
  }
  
  return 0;
}
