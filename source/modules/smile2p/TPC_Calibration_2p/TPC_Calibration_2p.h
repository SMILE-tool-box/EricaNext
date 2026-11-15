#ifndef TPC_Calibration_2p_h
#define TPC_Calibration_2p_h 1

#include <anlnext/BasicModule.hh>
#include <sstream>

#include "SaveFile.h"
#include "ReadData_2p.h"
#include "CalcFADC_2p.h"

using namespace anlnext;

class TPC_Calibration_2p : public BasicModule {
  DEFINE_ANL_MODULE(TPC_Calibration_2p, 1.0);

 public:
  TPC_Calibration_2p();
  ~TPC_Calibration_2p();

  ANLStatus mod_define() override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze() override;
  ANLStatus mod_finalize() override;

  double    Get_Energy()  const {return Energy;};


 private:
  const ReadData_2p       *RawData = nullptr;
  const CalcFADC_2p       *Q_TPC = nullptr;

  int                   max_anode, max_cathode, max_clock;
  int                   Na, Nc;
  int                   strip_num_chip_a, strip_num_chip_c;
  double                Energy;

  // SMILE_tool_box TPC_Calibration_2p::TPC_Anode_Cathode_Hit: per-event
  // anode-strip x cathode-strip coincidence count (reset every event).
  std::vector<int>      AnodeCathodeHit;   // [max_anode*max_cathode]
  // SMILE_tool_box TPC_Calibration_2p::TPC_Panel_Hit: per-event, per
  // (anode ADC panel, cathode ADC panel) aggregated hit count, built from
  // AnodeCathodeHit with a 4-strip edge exclusion on both axes.
  std::vector<int>      HitPanel;          // [Na*Nc]
  std::vector<int>      CountHitPanel;     // [Na], sum over j of HitPanel[i][j] with HitPanel[i][j] > 100
  std::vector<double>   cal_a, cal_b;
  double                Cal_Modify;
  TH2C                 *Coinci_A, *Coinci_C;
  std::string           PrmFile;
};

#endif
