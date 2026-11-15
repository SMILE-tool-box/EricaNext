#include "Reconstruct_2p.h"

ANLStatus Reconstruct_2p::mod_define(){
  return AS_OK;
}

ANLStatus Reconstruct_2p::mod_initialize(){
  if(!(exist_module("SaveFile")) ||
     !(exist_module("HA_Calc_2p")) ||
     !(exist_module("TPC_Calibration_2p")))
    return AS_QUIT;
  get_module("ReadData_2p",         &RawData);
  get_module("HA_Calc_2p",          &HA2p_data);
  get_module("TPC_Calibration_2p",  &TPC_FADC);
  if(exist_module("TOT_Skewness_2p"))
    get_module("TOT_Skewness_2p",   &TRA);

  define_evs("NotConformEnergy");
  
  SaveFile* saveModule = nullptr;
  get_module_NC("SaveFile", &saveModule);
  saveModule->cd();
  saveModule->GetDirectory()->mkdir("Reconstruct_2p");
  saveModule->GetDirectory()->cd("Reconstruct_2p");
  
  tree = new TTree("single_hit", "SM2p SingleHit");
  // cross-run comparison key: matches SMILE_tool_box Reconstruct_2p/s2t's anode_id
  tree->Branch("event_id", &event_id, "event_id/i");
  tree->Branch("sevt_id", &sevt_id, "sevt_id/i");
  tree->Branch("Eg", &Eg, "Eg/D");
  tree->Branch("Ke", &Ke, "Ke/D");
  tree->Branch("E0", &E0, "E0/D");
  tree->Branch("Pg", &Pg, "Pg[3]/D");
  tree->Branch("err_flag",  &err_flag,  "err_flag/I");
  tree->Branch("TOT_Area",  TOT_Area,   "TOT_Area[2]/D");
  tree->Branch("Pe_TRA",    Pe_TRA,     "Pe_TRA[6]/D");
  tree->Branch("Ve_TRA",    Ve_TRA,     "Ve_TRA[6]/D");
  tree->Branch("Len_TRA",   &Len_TRA,   "Len_TRA/D");
  tree->Branch("Ve_TRA_fit_status", Ve_TRA_fit_status, "Ve_TRA_fit_status[2]/I");
  tree->Branch("Ve_TRA_grad_err",   Ve_TRA_grad_err,   "Ve_TRA_grad_err[2]/D");
  tree->Branch("Track_Min", Track_Min,  "Track_Min[3]/D");
  tree->Branch("Track_Max", Track_Max,  "Track_Max[3]/D");
  tree->Branch("skewness_x",&skewness_x,"skewness_x/D");
  tree->Branch("skewness_y",&skewness_y,"skewness_y/D");
  tree->Branch("HA_Pixel",  HA_Pixel,   "HA_Pixel[8]/S");
  // not in SMILE_tool_box's s2t: gravity and MDF_2p-converted position of the
  // single-hit pixel, so the pixel maps can be drawn from the tree
  tree->Branch("HA_Gravity", HA_Gravity, "HA_Gravity[2]/D");
  tree->Branch("HA_MDF",     HA_MDF,     "HA_MDF[2]/D");
  tree->Branch("Hit_Flag_Num", &Hit_Flag_Num, "Hit_Flag_Num/S");
  tree->Branch("phi_kin",   &phi_deg,   "phi_kin/D");
  tree->Branch("psi_kin",   &psi_deg,   "psi_kin/D");
  tree->Branch("alpha_kin", &alpha_deg, "alpha_kin/D");
  tree->Branch("cos_phi",       &phi_kin,   "cos_phi/D");
  tree->Branch("cos_psi_kin",   &psi_kin,   "cos_psi_kin/D");
  tree->Branch("cos_alpha_kin", &alpha_kin, "cos_alpha_kin/D");
  tree->Branch("max_cos_alpha", &alpha_lim, "max_cos_alpha/D");
  
  return AS_OK;
}

ANLStatus Reconstruct_2p::mod_analyze(){
  // SMILE_tool_box Reconstruct_2p::Process() never skips the chain and always
  // fills its TTree row (815630 rows for 815630 raw events in the
  // reference file) -- Eg/Pg/Ee are read from banks that HA_Calc_s2 /
  // TPC_Calibration_2p set unconditionally every event (0xffffffff sentinel
  // on failure), and the SingleHit-only kinematics (phi/psi/alpha_kin) are
  // computed only when those banks are actually valid this event.
  const double SENTINEL = (double)0xffffffffu;

  event_id = RawData->Get_EventID();
  sevt_id  = RawData->Get_SEvtID();

  // SMILE_tool_box Reconstruct_2p::Process():
  //   SingleHit = Ee && Eg && !Eg2 && (Ve_THA || Ve_TRA)
  // (no THA module in this chain, Eg2 is never set by HA_Calc_s2).
  // Ee/Eg/Pg are only re-read from the banks when SingleHit; otherwise the
  // members keep the PREVIOUS event's values (so invalid rows of s2t carry
  // stale Eg/Pg/Ee) -- reproduced here by simply not touching them.
  bool eg_valid = evs("HA2p_EgValid") && !(HA2p_data->DetEng()->empty());
  bool ee_valid = evs("TPC_Ee_Valid");
  bool single_hit = eg_valid && ee_valid && evs("TOT_Ve_TRA");

  // read every event at the top of reference Process()
  Hit_Flag_Num = HA2p_data->Get_HitFlagNum();
  TOT_Area[0] = RawData->Get_NHitA();
  TOT_Area[1] = RawData->Get_NHitC();
  E0 = 0;       // reference's Et is reset to 0 every event
  Len_TRA = 0;  // reference resets Len_TRA every event

  // Observed in the reference ROOT file (2 of 20000 events: sevt 7904,
  // 16877): when Ee && Eg are valid but Ve_TRA is not, s2t carries the
  // fresh Ee/Eg/Pg/HA_Pixel of that event while err_flag and the angles are
  // left as they were. That matches the v2.20 structure
  //   if (outer) { read banks; if (SingleHit) {...} } else { err_flag=1; ... }
  // with the outer condition being Ee && Eg (without the Ve_TRA term), so
  // that is what is reproduced here.
  // (The Per03 reference instead leaves such rows stale when there is no 3D
  // coincidence at all -- sevt 26385, 26628 -- apparently a different
  // binary; this follows Per02, the reference this port is validated against.)
  bool outer = eg_valid && ee_valid;
  if(outer){
    Ke    = TPC_FADC->Get_Energy();
    Eg    = HA2p_data->DetEng()->at(0);
    Pg[0] = HA2p_data->Det_X()->at(0);
    Pg[1] = HA2p_data->Det_Y()->at(0);
    Pg[2] = HA2p_data->Det_Z()->at(0);
    for(int k=0; k<8; k++)  HA_Pixel[k] = HA2p_data->Get_HAPixel()[k];
    for(int k=0; k<2; k++){
      HA_Gravity[k] = HA2p_data->Get_HAGravity()[k];
      HA_MDF[k]     = HA2p_data->Get_HAMDF()[k];
    }
    if(TRA){
      for(int k=0; k<3; k++){
	Track_Min[k] = TRA->GetTrackMin()[k];
	Track_Max[k] = TRA->GetTrackMax()[k];
      }
      if(evs("TOT_Ve_TRA")){
	for(int k=0; k<3; k++){
	  Pe_TRA[k] = TRA->GetPe()[k];  Pe_TRA[k+3] = SENTINEL;
	  Ve_TRA[k] = TRA->GetVe()[k];  Ve_TRA[k+3] = SENTINEL;
	}
	Len_TRA = TRA->GetLen();
	for(int k=0; k<2; k++){
	  Ve_TRA_fit_status[k] = TRA->GetFitStatus()[k];
	  Ve_TRA_grad_err[k]   = TRA->GetGradErr()[k];
	}
      }
      else{
	// Per02 reference (sevt 7904, 16877): with no direction fit the
	// fit status reads 1 while the gradient error keeps its old value.
	// (The fit-status bank is not in the v2.20 source at all; the
	// reference binary is newer.)
	Ve_TRA_fit_status[0] = Ve_TRA_fit_status[1] = 1;
      }
      if(evs("TOT_Skewness_2p")){
	skewness_x = TRA->GetSkewnessX();
	skewness_y = TRA->GetSkewnessY();
      }
    }
  }

  if(single_hit){
    E0 = Ke + Eg;
    err_flag = 0;

    // SMLib Compton_common (mass_e = 510.9989461 keV), ERROR_DEG = -1000
    const double me = mc2;
    const double ERROR_DEG = -1000.;
    phi_kin   = 1. - me / (Eg + Ke) * Ke / Eg;
    psi_kin   = (1. + me / (Eg + Ke)) * sqrt(1. / (1. + 2. * me / Ke));
    alpha_kin = (1. - me / Eg) * sqrt(1. / (1. + 2. * me / Ke));
    phi_deg   = (-1. <= phi_kin   && phi_kin   <= 1.) ? TMath::ACos(phi_kin)   * TMath::RadToDeg() : ERROR_DEG;
    psi_deg   = (-1. <= psi_kin   && psi_kin   <= 1.) ? TMath::ACos(psi_kin)   * TMath::RadToDeg() : ERROR_DEG;
    // Alpha_kin() = Alpha_kin_rad()*RadToDeg, and Alpha_kin_rad returns
    // ERROR_DEG (-1000) in RADIANS when out of range -> -1000*RadToDeg.
    alpha_deg = ((-1. <= alpha_kin && alpha_kin <= 1.) ? TMath::ACos(alpha_kin) : ERROR_DEG) * TMath::RadToDeg();

    double k = E0/mc2;
    if(k > 1)
      alpha_lim = ((k-1)/(k+2)) * TMath::Sqrt((k*k-1)/(k*(k+2)));
    else
      alpha_lim = 0;

    if(TMath::Abs(alpha_kin) > 1)
      set_evs("NotConformEnergy");
  }
  else if(!outer){
    // reference: the else of the OUTER if
    err_flag = 1;
    phi_deg = psi_deg = alpha_deg = SENTINEL;
    phi_kin = psi_kin = alpha_kin = alpha_lim = SENTINEL;
    for(int k=0; k<3; k++){
      Ve_TRA[k] = SENTINEL;
      Track_Min[k] = Track_Max[k] = SENTINEL;
    }
  }

  tree->Fill();

  return AS_OK;
}

ANLStatus Reconstruct_2p::mod_finalize(){
  return AS_OK;
}


Reconstruct_2p::Reconstruct_2p(){
  // SMLib Compton_common::mass_e
  mc2 = 510.9989461;
  Eg = Ke = E0 = 0.;
  Pg[0] = Pg[1] = Pg[2] = 0.;
  err_flag = 1;
  phi_deg = psi_deg = alpha_deg = 0.;
  phi_kin = psi_kin = alpha_kin = alpha_lim = 0.;
  for(int k=0; k<6; k++)  Pe_TRA[k] = Ve_TRA[k] = 0.;
  for(int k=0; k<2; k++){ Ve_TRA_fit_status[k] = 0; Ve_TRA_grad_err[k] = 0.; }
  for(int k=0; k<3; k++)  Track_Min[k] = Track_Max[k] = 0.;
  for(int k=0; k<8; k++)  HA_Pixel[k] = 0;
  for(int k=0; k<2; k++)  HA_Gravity[k] = HA_MDF[k] = 0.;
  Len_TRA = skewness_x = skewness_y = 0.;
  Hit_Flag_Num = 0;
  TOT_Area[0] = TOT_Area[1] = 0.;
}

Reconstruct_2p::~Reconstruct_2p(){
}
