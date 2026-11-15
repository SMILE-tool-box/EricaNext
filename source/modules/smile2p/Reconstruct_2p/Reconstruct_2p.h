#ifndef Reconstruct_2p_h
#define Reconstruct_2p_h 1

#include <anlnext/BasicModule.hh>
#include <TTree.h>

#include "SaveFile.h"
#include "ReadData_2p.h"
#include "HA_Calc_2p.h"
#include "TPC_Calibration_2p.h"
#include "TOT_Skewness_2p.h"

using namespace anlnext;

class Reconstruct_2p : public BasicModule {
  DEFINE_ANL_MODULE(Reconstruct_2p, 1.0);

 public:
  Reconstruct_2p();
  ~Reconstruct_2p();

  ANLStatus mod_define() override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze() override;
  ANLStatus mod_finalize() override;
  
 private:
  TTree        *tree;
  unsigned      event_id;
  unsigned      sevt_id;
  double        Eg, Pg[3], Vg[3];
  double        Ke, Pe[3], Ve[3];
  double        E0, V0[3];
  double        phi_kin;
  double        psi_kin;
  double        alpha_kin, alpha_lim;
  double        mc2;
  // SMILE_tool_box Reconstruct_2p/s2t: err_flag (0 = SingleHit reconstructed)
  // and phi_kin/psi_kin/alp_kin in DEGREES (Compton_common::angle_scatter,
  // angle_recoil, Alpha_kin), 0xffffffff when not reconstructed.
  int           err_flag;
  double        phi_deg, psi_deg, alpha_deg;
  // more SMILE_tool_box s2t branches, same names / shapes / update rules
  double        Pe_TRA[6], Ve_TRA[6], Len_TRA;
  int           Ve_TRA_fit_status[2];
  double        Ve_TRA_grad_err[2];
  double        Track_Min[3], Track_Max[3];
  double        skewness_x, skewness_y;
  short         HA_Pixel[8];
  double        HA_Gravity[2], HA_MDF[2];
  short         Hit_Flag_Num;
  double        TOT_Area[2];

  const ReadData_2p*         RawData = nullptr;
  const HA_Calc_2p*          HA2p_data = nullptr;
  const TPC_Calibration_2p*  TPC_FADC = nullptr;
  const TOT_Skewness_2p*     TRA = nullptr;
};

#endif
