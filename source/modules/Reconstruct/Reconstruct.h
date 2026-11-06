#ifndef Reconstruct_h
#define Reconstruct_h 1

#include <anlnext/BasicModule.hh>
#include <TTree.h>

#include "SaveFile.h"
#include "ReadData.h"
#include "PSA_Event.h"
#include "TPC_Calibration.h"

using namespace anlnext;

class Reconstruct : public BasicModule {
  DEFINE_ANL_MODULE(Reconstruct, 1.0);

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
  double        Eg, Pg[3], Vg[3];
  double        Ke, Pe[3], Ve[3];
  double        E0, V0[3];
  double        phi_kin;
  double        psi_kin;
  double        alpha_kin, alpha_lim;
  double        mc2;
  
  const ReadData*         RawData = nullptr;
  const PSA_Event*        PSA_data = nullptr;   // SMILE-3 scintillator (SMILE-2+: Reconstruct_2p with HA_Calc_2p)
  const TPC_Calibration*  TPC_FADC = nullptr;
};

#endif
