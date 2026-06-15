#include "HA_Calc.h"

bool HA_Calc::ReadCalPrm(){
  if(exist_module("SaveFile")){
    Spec_PMT = new TH2D* [N_PMT * (ha_id->size())];

    for(unsigned i=0; i<ha_id->size(); i++){
      for(int j=0; j<N_PMT; j++){
	char histname[100];
	sprintf(histname, "Spectrum_HA%02d_PMT%d", (*ha_id)[i], j);
		
	if(cal_flag){
	  Spec_PMT[N_PMT*i+j]
	    = new TH2D(histname, histname,
		       4096, -0.5, 16383.5,  64, -0.5, 63.5);
	  Spec_PMT[N_PMT*i+j]->SetXTitle("[ADU]");
	  Spec_PMT[N_PMT*i+j]->SetYTitle("pixel id");
	}
	else{
	  Spec_PMT[N_PMT*i+j]
	    = new TH2D(histname, histname,
		       2000, 0, 4000,  64, -0.5, 63.5);
	  Spec_PMT[N_PMT*i+j]->SetXTitle("Energy [keV]");
	  Spec_PMT[N_PMT*i+j]->SetYTitle("Pixel ID");	  
	}
      }
    }
  }

  if(!cal_flag){
    cal_a = new double [64* N_PMT * (ha_id->size())];
    cal_b = new double [64* N_PMT * (ha_id->size())];

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
