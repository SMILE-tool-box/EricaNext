#include "HA_Calc_2p.h"

ANLStatus HA_Calc_2p::mod_define(){
  define_parameter("parameter_dir", &mod_class::PrmDir);
  define_parameter("Flag_Modify_PMT_Gain", &mod_class::modify_pmt_gain);
  define_parameter("Modified_Parameter_Data_Dir", &mod_class::ModifiedPrmDir);
  define_parameter("SingleHit_Eth", &mod_class::SingleHit_Eth);
  define_parameter("SingleHit_ADCth", &mod_class::SingleHit_ADCth);
  define_parameter("Emu_Mode", &mod_class::emu_mode);
  set_parameter_description("1 for simulated data (EmuWrite2p / RawWriter2p): PMT / pixel IDs of the emulator are renumbered before the position (SMILE_tool_box HA_Emu_ID_Modification)");

  return AS_OK;
}

ANLStatus HA_Calc_2p::mod_initialize(){
  if(!(exist_module("HA_CenterGravity_2p")))
    return AS_QUIT;
  get_module("HA_CenterGravity_2p", &HA_Grv);
  get_module("ReadData_2p", &RawData);
  ha_id = RawData->Get_HA2pID();
  N_PMT = RawData->Get_Npmt_HA2p();

  ReadPrm4MDF();
  ReadCalPrm();

  define_evs("HA_OutOfMDFrange");
  define_evs("HA_NoHits");
  define_evs("HA2p_LowEnergy");
  define_evs("HA2p_Impure");
  // SMILE_tool_box HA_Calc_s2::Set_HA_Pixel(): evs->Set("Eg") only when
  // Hit_Flag_Num==1 && Hit_E_Num==0 -- this is what Reconstruct_2p checks
  // instead of any SKIP from this module (reference's HA_Calc_s2 never
  // returns ANL_SKIP).
  define_evs("HA2p_EgValid");

  return AS_OK;
}

ANLStatus HA_Calc_2p::mod_analyze(){
  det_eng.clear();
  det_x.clear();  det_y.clear();  det_z.clear();

  // calibrated energy / position of every gravity-valid pixel
  // (SMILE_tool_box: HA_Hit_Flag==1 pixels, the only ones HA_Pixel_XYE() runs on)
  std::vector<double> all_eng, all_x, all_y, all_z, all_grv, all_mdf;
  std::vector<short>  all_pix;   // 4 per hit: HA index, PMT, x_id, y_id

  const std::vector<int> *hit_id = HA_Grv->Hit_HA();
  for(unsigned id=0; id<hit_id->size(); id++){
    int ha, pmt;
    double eng, grv[2];
    HA_Grv->Hit_Info((*hit_id)[id], ha, pmt, eng, grv[0], grv[1]);

    int tmp = std::distance(ha_id->begin(),
			    std::find(ha_id->begin(), ha_id->end(), ha));
    double rx = mdf_x[tmp]->ConvertMDF(pmt, grv);
    double ry = mdf_y[tmp]->ConvertMDF(pmt, grv);
    all_grv.push_back(grv[0]);  all_grv.push_back(grv[1]);
    all_mdf.push_back(rx);      all_mdf.push_back(ry);
    // SMILE_tool_box HA_Calc_s2::HA_Pixel_XYE(): clamps unconditionally,
    // with no out-of-range rejection at all.
    if(TMath::Abs(rx) > 0.5 || TMath::Abs(ry) > 0.5)
      set_evs("HA_OutOfMDFrange");

    if(rx < -0.4)  rx =-0.39;
    if(rx >  0.4)  rx = 0.39;
    if(ry < -0.4)  ry =-0.39;
    if(ry >  0.4)  ry = 0.39;
    short x_id = TMath::FloorNint(10 * rx + 4);
    short y_id = TMath::FloorNint(10 * ry + 4);
    double det_pos[3] = {pos_x[tmp], pos_y[tmp], pos_z[tmp]};
    
    unsigned cal_idx = 64*(N_PMT*tmp+pmt)+8*x_id+y_id;
    // SMILE_tool_box HA_Calc_s2::HA_Pixel_XYE():
    //   eng = cal_a*eng + cal_b; eng *= 662/(cal_a*cal_c + cal_b);
    eng = cal_a[cal_idx] * eng + cal_b[cal_idx];
    eng *= 662. / (cal_a[cal_idx] * cal_c[cal_idx] + cal_b[cal_idx]);

    // SMILE_tool_box HA_Calc_s2::HA_Emu_ID_Modification() (Emu_Mode): the emulator numbers
    // PMTs / pixels differently; renumber before the position (energy above uses the original IDs)
    if(emu_mode){
      switch(pmt){
      case 0: pmt = 1; x_id = 7 - x_id; y_id = 7 - y_id; break;
      case 1: pmt = 5; x_id = 7 - x_id; y_id = 7 - y_id; break;
      case 3:          x_id = 7 - x_id; y_id = 7 - y_id; break;
      case 5: pmt = 0; break;
      default: break;
      }
    }

    // SMILE_tool_box HA_Calc_s2::HA_Calc_Position():
    //   ha_x = 54*(pmt%2-0.5) + 6*(x_id-3.5)
    //   ha_y = 54*(1-pmt/2)   + 6*(y_id-3.5)
    double ha_x = 54 * ((pmt % 2) - 0.5) + 6 * (x_id - 3.5);
    double ha_y = 54 * (1 - pmt / 2)     + 6 * (y_id - 3.5);
    det_pos[0] += dir_xx[tmp] * ha_x + dir_xy[tmp] * ha_y;
    det_pos[1] += dir_yx[tmp] * ha_x + dir_yy[tmp] * ha_y;
    det_pos[2] += dir_zx[tmp] * ha_x + dir_zy[tmp] * ha_y;

    all_eng.push_back(eng);
    all_pix.push_back(tmp);  all_pix.push_back(pmt);
    all_pix.push_back(x_id); all_pix.push_back(y_id);
    all_x.push_back(det_pos[0]);
    all_y.push_back(det_pos[1]);
    all_z.push_back(det_pos[2]);
  }
  if(hit_id->empty())
    set_evs("HA_NoHits");

  // SMILE_tool_box HA_Calc_s2::Count_Hit_Num():
  //   Hit_Flag_Num = #pixels with HA_Hit_Flag==1 && HA_Pixel_E > Eth
  //   Hit_E_Num    = #pixels with HA_Hit_Flag!=1 && HA_RAW_E   > ADCth
  // Note the Eth cut is applied to EACH gravity-valid pixel before counting,
  // so weak satellite pixels (crosstalk etc.) below Eth don't count as hits.
  for(int k=0; k<8; k++)  ha_pixel_[k] = -10000;
  int hit_flag_num = 0, chosen = -1;
  for(unsigned k=0; k<all_eng.size(); k++)
    if(all_eng[k] > SingleHit_Eth){ hit_flag_num++; chosen = k; }
  hit_flag_num_ = hit_flag_num;

  int n_pixel = HA_Grv->Get_NHA() * HA_Grv->Get_NPMT();
  std::vector<bool> is_hit(n_pixel, false);
  for(int idx : *hit_id)  is_hit[idx] = true;
  int hit_e_num = 0;
  for(int i=0; i<n_pixel; i++)
    if(!is_hit[i] && HA_Grv->Get_RawE(i) > SingleHit_ADCth)  hit_e_num++;

  // SMILE_tool_box HA_Calc_s2::Set_HA_Pixel(): Eg is set (evs "Eg") only
  // when Hit_Flag_Num==1 && Hit_E_Num==0; otherwise Eg/Pg stay at the
  // 0xffffffff sentinel and the chain continues (no ANL_SKIP).
  if(hit_flag_num == 0 && !all_eng.empty())  set_evs("HA2p_LowEnergy");
  if(hit_e_num > 0)                          set_evs("HA2p_Impure");

  if(hit_flag_num == 1 && hit_e_num == 0){
    det_eng.push_back(all_eng[chosen]);
    det_x.push_back(all_x[chosen]);
    det_y.push_back(all_y[chosen]);
    det_z.push_back(all_z[chosen]);
    for(int k=0; k<4; k++)  ha_pixel_[k] = all_pix[4*chosen+k];
    for(int k=0; k<2; k++){
      ha_grv_[k] = all_grv[2*chosen+k];
      ha_mdf_[k] = all_mdf[2*chosen+k];
    }
    set_evs("HA2p_EgValid");
  }

  return AS_OK;
}

ANLStatus HA_Calc_2p::mod_finalize(){
  for(unsigned i=0; i<ha_id->size(); i++){
    delete mdf_x[i];  delete mdf_y[i];
  }
  delete [] mdf_x;  delete [] mdf_y;
  
  return AS_OK;
}


HA_Calc_2p::HA_Calc_2p(){
  PrmDir = "./";
  modify_pmt_gain = false;
  emu_mode = false;
  ModifiedPrmDir = "./";
  SingleHit_Eth = 50.;
  SingleHit_ADCth = 1000.;
}

HA_Calc_2p::~HA_Calc_2p(){}
