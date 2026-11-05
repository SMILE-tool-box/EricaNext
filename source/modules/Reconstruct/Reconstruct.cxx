#include "Reconstruct.h"

#include <iostream>

ANLStatus Reconstruct::mod_define(){
  return AS_OK;
}

ANLStatus Reconstruct::mod_initialize(){
  // the scintillator hits: PSA_Event (SMILE-3 PSA) when in the chain, else HA_Calc (SMILE-2+)
  if(!(exist_module("SaveFile")) ||
     !(exist_module("HA_Calc") || exist_module("PSA_Event")) ||
     !(exist_module("TPC_Calibration"))){
    std::cerr << "Reconstruct: needs SaveFile, TPC_Calibration and PSA_Event (or HA_Calc)" << std::endl;
    return AS_QUIT;
  }
  get_module("ReadData",         &RawData);
  if(exist_module("PSA_Event"))  get_module("PSA_Event", &PSA_data);
  else                           get_module("HA_Calc",   &HA2p_data);
  get_module("TPC_Calibration",  &TPC_FADC);

  define_evs("NotConformEnergy");
  
  SaveFile* saveModule = nullptr;
  get_module_NC("SaveFile", &saveModule);
  saveModule->cd();
  saveModule->GetDirectory()->mkdir("Reconstruct");
  saveModule->GetDirectory()->cd("Reconstruct");
  
  tree = new TTree("single_hit", "SM2p SingleHit");
  tree->Branch("evt_id", &evt_id, "evt_id/i");
  tree->Branch("Eg", &Eg, "Eg/D");
  tree->Branch("Ke", &Ke, "Ke/D");
  tree->Branch("E0", &E0, "E0/D");
  tree->Branch("Pg", &Pg, "Pg[3]/D");
  tree->Branch("cos_phi",       &phi_kin,   "cos_phi/D");
  tree->Branch("cos_psi_kin",   &psi_kin,   "cos_psi_kin/D");
  tree->Branch("cos_alpha_kin", &alpha_kin, "cos_alpha_kin/D");
  tree->Branch("max_cos_alpha", &alpha_lim, "max_cos_alpha/D");
  
  return AS_OK;
}

ANLStatus Reconstruct::mod_analyze(){
  const bool single = PSA_data ? evs("PSA_SingleHit") : evs("HA2p_SingleHit");
  if(single){
    evt_id = RawData ? RawData->Get_EventID_coincidence() : 0;
    Eg    = PSA_data ? PSA_data->DetEng()->at(0) : HA2p_data->DetEng()->at(0);
    Ke    = TPC_FADC->Get_Energy();
    E0    = Eg + Ke;
    Pg[0] = PSA_data ? PSA_data->Det_X()->at(0) : HA2p_data->Det_X()->at(0);
    Pg[1] = PSA_data ? PSA_data->Det_Y()->at(0) : HA2p_data->Det_Y()->at(0);
    Pg[2] = PSA_data ? PSA_data->Det_Z()->at(0) : HA2p_data->Det_Z()->at(0);
    phi_kin = 1 - mc2*(1/Eg - 1/E0);
    psi_kin = (1 + mc2/E0) * TMath::Sqrt(Ke/(Ke+2*mc2));
    alpha_kin = (1 - mc2/Eg) * TMath::Sqrt(Ke/(Ke+2*mc2));

    double k = E0/mc2;
    if(k > 1)
      alpha_lim = ((k-1)/(k+2)) * TMath::Sqrt((k*k-1)/(k*(k+2)));
    else
      alpha_lim = 0;

    if(TMath::Abs(alpha_kin) > 1){
      set_evs("NotConformEnergy");
      return AS_SKIP;
    }
    
    tree->Fill();
  }
  
  return AS_OK;
}

ANLStatus Reconstruct::mod_finalize(){
  return AS_OK;
}


Reconstruct::Reconstruct(){
  mc2 = 510.99895;
}

Reconstruct::~Reconstruct(){
}
