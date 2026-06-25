#ifndef TPC_Calibration_h
#define TPC_Calibration_h 1

#include <anlnext/BasicModule.hh>
#include <sstream>
#include <vector>
#include <TTree.h>

#include "SaveFile.h"
#include "ReadData.h"
#include "CalcFADC.h"

using namespace anlnext;

class TPC_Calibration : public BasicModule {
  DEFINE_ANL_MODULE(TPC_Calibration, 1.0);

 public:
  TPC_Calibration();
  ~TPC_Calibration();

  ANLStatus mod_define() override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze() override;
  ANLStatus mod_finalize() override;

  double    Get_Energy()  const {return Energy;};

 private:
  const ReadData       *RawData = nullptr;
  const CalcFADC       *Q_TPC = nullptr;

  int                   max_anode, max_cathode, max_clock;
  int                   Na, Nc;
  double                Energy;
  std::vector<int>      HitPanel;
  std::vector<double>   cal_a, cal_b;
  TH2D                 *Image_XY;
  // TH2D                 *Image_XYZ;
  TH2C                 *Coinci_A, *Coinci_C;
  std::string           PrmFile;
  bool                  no_cal_data;
  bool                  Coinci_TH;
  bool                  Image_XY_TH;
  bool                  Spectrum_TH;
  TH1D                 *Spectrum;
};

#endif
