#ifndef Reconstruct_h
#define Reconstruct_h 1

#include <vector>

#include <anlnext/BasicModule.hh>
#include <TTree.h>

#include "SaveFile.h"
#include "ReadData.h"
#include "PSA_Event.h"
#include "TPC_Calibration.h"
#include "TOT_Skewness.h"

using namespace anlnext;

class Reconstruct : public BasicModule {
  DEFINE_ANL_MODULE(Reconstruct, 1.1);

 public:
  Reconstruct();
  ~Reconstruct();

  ANLStatus mod_define() override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze() override;
  ANLStatus mod_finalize() override;

 private:
  TTree        *tree;
  unsigned      evt_id;   // trigger counter of the event (ReadData)
  double        Eg, Pg[3];
  double        Ke, E0;
  double        phi_kin;
  double        psi_kin;
  double        alpha_kin, alpha_lim;
  double        mc2;

  // with the electron track (TOT_Skewness): SMILE_tool_box Reconstruct (TRA)
  int           has_track;          // 1: Pe and Ve of TOT_Skewness are valid this event
  double        Pe[3];              // scattering point (start of the track) [mm]
  double        Ve[3];              // direction of the recoil electron (unit vector)
  double        Vg[3];              // direction of the scattered gamma, Pg - Pe (unit vector)
  double        cos_alpha_geo;      // Vg . Ve
  double        delta_cos_alpha;    // cos_alpha_geo - cos_alpha_kin
  double        Vi[3];              // direction of the incident gamma (unit, from the source to the detector)
  double        cos_arm, arm, spd;  // with source_position: ARM [deg] and SPD [deg]

  std::vector<double> SourcePosition;  // [mm] in the detector frame; empty: no ARM / SPD

  void Track();

  const ReadData*         RawData = nullptr;
  const PSA_Event*        PSA_data = nullptr;   // SMILE-3 scintillator (SMILE-2+: Reconstruct_2p with HA_Calc_2p)
  const TPC_Calibration*  TPC_FADC = nullptr;
  const TOT_Skewness*     TRA = nullptr;        // optional: without it only the kinematic angles
};

#endif
