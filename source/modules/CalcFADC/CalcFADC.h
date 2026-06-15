#ifndef CalcFADC_h
#define CalcFADC_h 1

#include <anlnext/BasicModule.hh>
#include <sstream>
#include <thread>
#include <mutex>
#include <TMath.h>

#include "SaveFile.h"
#include "ReadData.h"

using namespace anlnext;

class CalcFADC : public BasicModule {
  DEFINE_ANL_MODULE(CalcFADC, 1.0);

 public:
  CalcFADC();
  ~CalcFADC();
  
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
  TH2D             *BaseAnode, *BaseCathode;
  TH2D             *ChargeAnode, *ChargeCathode;
  TH1D            **Spec1Hit_A, **Spec1Hit_C;
  
  const ReadData*          RawData = nullptr;
};

#endif
