#include "HA_Calc.h"

ANLStatus HA_Calc::mod_define(){
  define_parameter("parameter_dir", &mod_class::PrmDir);
  define_parameter("Cal_Mode", &mod_class::cal_flag);
  
  return AS_OK;
}

ANLStatus HA_Calc::mod_initialize(){
  if(!(exist_module("HA_CenterGravity")))
    return AS_QUIT;
  get_module("HA_CenterGravity", &HA_Grv);
  get_module("ReadData", &RawData);
  ha_id = RawData->Get_HA2pID();
  N_PMT = RawData->Get_Npmt_HA2p();

  if(exist_module("SaveFile")){
    SaveFile* saveModule = nullptr;
    get_module_NC("SaveFile", &saveModule);
    saveModule->cd();
    saveModule->GetDirectory()->mkdir("HA_Calc");
    saveModule->GetDirectory()->cd("HA_Calc");
  }

  ReadPrm4MDF();
  ReadCalPrm();

  define_evs("HA_OutOfMDFrange");
  define_evs("HA_NoHits");
  
  return AS_OK;
}

ANLStatus HA_Calc::mod_analyze(){
  det_eng.clear();
  det_x.clear();  det_y.clear();  det_z.clear();
  
  const std::vector<int> *hit_id = HA_Grv->Hit_HA();
  for(unsigned id=0; id<hit_id->size(); id++){
    int ha, pmt;
    double eng, grv[2];
    HA_Grv->Hit_Info((*hit_id)[id], ha, pmt, eng, grv[0], grv[1]);

    int tmp = std::distance(ha_id->begin(),
			    std::find(ha_id->begin(), ha_id->end(), ha));
    double rx = mdf_x[tmp]->ConvertMDF(pmt, grv);
    double ry = mdf_y[tmp]->ConvertMDF(pmt, grv);
    if(TMath::Abs(rx) > 0.5 || TMath::Abs(ry) > 0.5){
      set_evs("HA_OutOfMDFrange");
      return AS_SKIP;
    }

    if(rx < -0.4)  rx =-0.39;
    if(rx >  0.4)  rx = 0.39;
    if(ry < -0.4)  ry =-0.39;
    if(ry >  0.4)  ry = 0.39;
    short x_id = TMath::FloorNint(10 * rx + 4);
    short y_id = TMath::FloorNint(10 * ry + 4);
    short pixel = x_id*8 + y_id;
    double det_pos[3] = {pos_x[tmp], pos_y[tmp], pos_z[tmp]};
    
    if(!cal_flag){
      eng = cal_a[64*(N_PMT*tmp+pmt)+8*x_id+y_id] * eng
	+ cal_b[64*(N_PMT*tmp+pmt)+8*x_id+y_id];

      double ha_x = 6 * (x_id - 3.5);
      double ha_y = 6 * (y_id - 3.5);
      det_pos[0] += dir_xx[tmp] * ha_x + dir_xy[tmp] * ha_y;
      det_pos[1] += dir_yx[tmp] * ha_x + dir_yy[tmp] * ha_y;
      det_pos[2] += dir_zx[tmp] * ha_x + dir_zy[tmp] * ha_y;
    }
    
    if(exist_module("SaveFile")){
      MDF_Image[N_PMT*tmp+pmt]->Fill(rx, ry);
      Spec_PMT[N_PMT*tmp+pmt]->Fill(eng, pixel);
    }

    if(eng > 0){
      det_eng.push_back(eng);
      det_x.push_back(det_pos[0]);
      det_y.push_back(det_pos[1]);
      det_z.push_back(det_pos[2]);
    }
  }
  if(det_eng.size() == 0){
    set_evs("HA_NoHits");
    return AS_SKIP;
  }
  
  return AS_OK;
}

ANLStatus HA_Calc::mod_finalize(){
  for(unsigned i=0; i<ha_id->size(); i++){
    delete mdf_x[i];  delete mdf_y[i];
  }
  delete [] mdf_x;  delete [] mdf_y;
  
  return AS_OK;
}


HA_Calc::HA_Calc(){
  PrmDir = "./";
  cal_flag = 1;
}

HA_Calc::~HA_Calc(){}
