#ifndef CalcFADC_2p_h
#define CalcFADC_2p_h 1

#include <anlnext/BasicModule.hh>
#include <sstream>
#include <thread>
#include <mutex>
#include <TMath.h>

#include "SaveFile.h"
#include "ReadData_2p.h"

using namespace anlnext;

class CalcFADC_2p : public BasicModule {
  DEFINE_ANL_MODULE(CalcFADC_2p, 1.0);

 public:
  CalcFADC_2p();
  ~CalcFADC_2p();
  
  ANLStatus mod_define() override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze() override;
  ANLStatus mod_finalize() override;

  void      IntegWave(bool, unsigned);
  int       Get_Nadc_A()  const {
    int N = RawData->Get_AnodeID()->size();
    return 4 * N;
  };
  int       Get_Nadc_C()  const {
    int N = RawData->Get_CathodeID()->size();
    return 4 * N;
  };
  double    Get_Qanode(int ch)     const {return Q_anode[ch];};
  double    Get_Qcathode(int ch)   const {return Q_cathode[ch];};
  
 private:
  double           *Q_anode, *Q_cathode;
  double            base_window, threshold;
  // SMILE_tool_box CalcFADC_2p: charge *= ADC_to_mV * Clock_to_nsec / 50.
  double            ADC_to_mV, Clock_to_nsec;
  // SMILE_tool_box CalcFADC_2p: integration window is [Range_min, Range_max),
  // NOT [base_window, Nclk) -- defaults 30/500, not registered in this
  // run's setting.mac so the code defaults apply.
  double            Range_min, Range_max;
  
  const ReadData_2p*          RawData = nullptr;
};

#endif
