#ifndef HA_Calc_h
#define HA_Calc_h 1

#include <anlnext/BasicModule.hh>
#include <sstream>
#include <algorithm>

#include "SaveFile.h"
#include "ReadData.h"
#include "HA_CenterGravity.h"
#include "MDF.h"

using namespace anlnext;

class HA_Calc : public BasicModule {
  DEFINE_ANL_MODULE(HA_Calc, 1.0);

 public:
  HA_Calc();
  ~HA_Calc();

  ANLStatus mod_define() override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze() override;
  ANLStatus mod_finalize() override;

  const std::vector<double>*  DetEng()   const {return &det_eng;};
  const std::vector<double>*  Det_X()    const {return &det_x;};
  const std::vector<double>*  Det_Y()    const {return &det_y;};
  const std::vector<double>*  Det_Z()    const {return &det_z;};

  
 private:
  std::string       PrmDir;
  int               N_HA, N_PMT;
  MDF             **mdf_x, **mdf_y;
  TH2D            **MDF_Image, **Spec_PMT;
  bool              cal_flag;
  double           *cal_a, *cal_b;
  double           *pos_x, *pos_y, *pos_z;
  double           *dir_xx, *dir_xy, *dir_yx, *dir_yy, *dir_zx, *dir_zy;  
  
  const std::vector<int>* ha_id;
  const ReadData*         RawData = nullptr;
  const HA_CenterGravity* HA_Grv = nullptr;
  std::vector<double>     det_eng;
  std::vector<double>     det_x;
  std::vector<double>     det_y;
  std::vector<double>     det_z;
  
  bool              ReadPrm4MDF();
  bool              ReadCalPrm();
};

#endif
