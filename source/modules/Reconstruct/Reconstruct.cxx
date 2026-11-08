#include "Reconstruct.h"

#include <cmath>
#include <iostream>

#include <TMath.h>
#include <TVector3.h>

namespace {
const double SENTINEL = static_cast<double>(0xffffffffu);   // not reconstructed (SMILE_tool_box)
}

ANLStatus Reconstruct::mod_define(){
  define_parameter("source_position", &mod_class::SourcePosition);
  return AS_OK;
}

ANLStatus Reconstruct::mod_initialize(){
  // the scintillator hits: PSA_Event (SMILE-3 PSA). SMILE-2+ data: Reconstruct_2p
  if(!(exist_module("SaveFile")) ||
     !(exist_module("PSA_Event")) ||
     !(exist_module("TPC_Calibration"))){
    std::cerr << "Reconstruct: needs SaveFile, TPC_Calibration and PSA_Event" << std::endl;
    return AS_QUIT;
  }
  get_module("ReadData",         &RawData);
  get_module("PSA_Event",        &PSA_data);
  get_module("TPC_Calibration",  &TPC_FADC);
  if(exist_module("TOT_Skewness"))
    get_module("TOT_Skewness",   &TRA);
  else
    std::cerr << "Reconstruct: no TOT_Skewness in the chain: only the kinematic angles (no track direction)" << std::endl;
  if(!SourcePosition.empty() && SourcePosition.size() != 3){
    std::cerr << "Reconstruct: source_position needs 3 values [mm] (or none)" << std::endl;
    return AS_QUIT;
  }

  define_evs("NotConformEnergy");

  SaveFile* saveModule = nullptr;
  get_module_NC("SaveFile", &saveModule);
  saveModule->cd();
  saveModule->GetDirectory()->mkdir("Reconstruct");
  saveModule->GetDirectory()->cd("Reconstruct");

  tree = new TTree("single_hit", "SMILE-3 SingleHit");
  tree->Branch("evt_id", &evt_id, "evt_id/i");
  tree->Branch("Eg", &Eg, "Eg/D");
  tree->Branch("Ke", &Ke, "Ke/D");
  tree->Branch("E0", &E0, "E0/D");
  tree->Branch("Pg", &Pg, "Pg[3]/D");
  tree->Branch("cos_phi",       &phi_kin,   "cos_phi/D");
  tree->Branch("cos_psi_kin",   &psi_kin,   "cos_psi_kin/D");
  tree->Branch("cos_alpha_kin", &alpha_kin, "cos_alpha_kin/D");
  tree->Branch("max_cos_alpha", &alpha_lim, "max_cos_alpha/D");
  if(TRA){
    tree->Branch("has_track",       &has_track,       "has_track/I");
    tree->Branch("Pe",              Pe,               "Pe[3]/D");
    tree->Branch("Ve",              Ve,               "Ve[3]/D");
    tree->Branch("Vg",              Vg,               "Vg[3]/D");
    tree->Branch("cos_alpha_geo",   &cos_alpha_geo,   "cos_alpha_geo/D");
    tree->Branch("delta_cos_alpha", &delta_cos_alpha, "delta_cos_alpha/D");
    tree->Branch("Vi",              Vi,               "Vi[3]/D");
    if(SourcePosition.size() == 3){
      tree->Branch("cos_arm", &cos_arm, "cos_arm/D");
      tree->Branch("arm",     &arm,     "arm/D");
      tree->Branch("spd",     &spd,     "spd/D");
    }
  }

  return AS_OK;
}

ANLStatus Reconstruct::mod_analyze(){
  // the scintillator single hit, and a TPC energy (TPC_Calibration; CalcFADC does not skip events)
  const bool single = evs("PSA_SingleHit") && evs("TPC_Ee_Valid");
  if(single){
    evt_id = RawData ? RawData->Get_EventID_coincidence() : 0;
    Eg    = PSA_data->DetEng()->at(0);
    Ke    = TPC_FADC->Get_Energy();
    E0    = Eg + Ke;
    Pg[0] = PSA_data->Det_X()->at(0);
    Pg[1] = PSA_data->Det_Y()->at(0);
    Pg[2] = PSA_data->Det_Z()->at(0);
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

    if(TRA) Track();

    tree->Fill();
  }

  return AS_OK;
}

void Reconstruct::Track(){
  // SMILE_tool_box Reconstruct (TRA, SingleHit) and SMLib Compton_common:
  //   g = Pg - Pe, e = Ve (TOT_Skewness gives a direction of any length), cos_alpha_geo = g_unit . e_unit,
  //   incident direction = (Eg g_unit + p_e e_unit) / |...|, p_e = sqrt(Ke (Ke + 2 mc2)) (recon_vector),
  //   ARM = angle(Pe - source, g) - phi_kin, SPD = signed angle between the planes (source, g) and (incident, g).
  has_track = 0;
  for(int i = 0; i < 3; i++)
    Pe[i] = Ve[i] = Vg[i] = Vi[i] = SENTINEL;
  cos_alpha_geo = delta_cos_alpha = cos_arm = arm = spd = SENTINEL;

  if(!evs("TOT_Ve_TRA")) return;
  const double* pe = TRA->GetPe();
  const double* ve = TRA->GetVe();
  for(int i = 0; i < 3; i++)
    if(pe[i] == SENTINEL || ve[i] == SENTINEL || !std::isfinite(pe[i]) || !std::isfinite(ve[i])) return;
  TVector3 p_e(pe[0], pe[1], pe[2]), e(ve[0], ve[1], ve[2]);
  TVector3 g = TVector3(Pg[0], Pg[1], Pg[2]) - p_e;
  if(e.Mag() == 0 || g.Mag() == 0) return;

  TVector3 e_unit = e.Unit(), g_unit = g.Unit();
  TVector3 inc = (Eg * g_unit + TMath::Sqrt(Ke * (Ke + 2 * mc2)) * e_unit).Unit();
  has_track = 1;
  p_e.GetXYZ(Pe);
  e_unit.GetXYZ(Ve);
  g_unit.GetXYZ(Vg);
  inc.GetXYZ(Vi);
  cos_alpha_geo   = g_unit.Dot(e_unit);
  delta_cos_alpha = cos_alpha_geo - alpha_kin;

  if(SourcePosition.size() == 3){
    TVector3 src = (p_e - TVector3(SourcePosition[0], SourcePosition[1], SourcePosition[2])).Unit();
    cos_arm = src.Dot(g_unit);
    if(TMath::Abs(phi_kin) <= 1)
      arm = (TMath::ACos(cos_arm) - TMath::ACos(phi_kin)) * TMath::RadToDeg();
    TVector3 n_src = src.Cross(g_unit).Unit(), n_inc = inc.Cross(g_unit).Unit();
    double c = n_src.Dot(n_inc);
    if(TMath::Abs(c) <= 1)
      spd = TMath::Sign(1., g_unit.Dot(n_src.Cross(n_inc))) * TMath::ACos(c) * TMath::RadToDeg();
  }
}

ANLStatus Reconstruct::mod_finalize(){
  return AS_OK;
}


Reconstruct::Reconstruct(){
  mc2 = 510.99895;
}

Reconstruct::~Reconstruct(){
}
