#ifndef HA_Calc_2p_h
#define HA_Calc_2p_h 1

#include <anlnext/BasicModule.hh>
#include <sstream>
#include <algorithm>

#include "SaveFile.h"
#include "ReadData_2p.h"
#include "HA_CenterGravity_2p.h"
#include "MDF_2p.h"

using namespace anlnext;

class HA_Calc_2p : public BasicModule {
  DEFINE_ANL_MODULE(HA_Calc_2p, 1.0);

 public:
  HA_Calc_2p();
  ~HA_Calc_2p();

  ANLStatus mod_define() override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze() override;
  ANLStatus mod_finalize() override;

  const std::vector<double>*  DetEng()   const {return &det_eng;};
  const std::vector<double>*  Det_X()    const {return &det_x;};
  const std::vector<double>*  Det_Y()    const {return &det_y;};
  const std::vector<double>*  Det_Z()    const {return &det_z;};
  // SMILE_tool_box HA_Calc_s2 banks "Hit_Flag_Num" (every event) and
  // "HA_Pixel" [2][4] = {HA index, PMT, pixel X, pixel Y}, -10000 if unset.
  short                       Get_HitFlagNum() const {return hit_flag_num_;};
  const short*                Get_HAPixel()    const {return ha_pixel_;};
  // gravity (HA_CenterGravity_2p) and MDF_2p-converted position (before the
  // +-0.39 clamp used for the pixel id) of the single-hit pixel
  const double*               Get_HAGravity()  const {return ha_grv_;};
  const double*               Get_HAMDF()      const {return ha_mdf_;};

  
 private:
  std::string       PrmDir;
  int               N_HA, N_PMT;
  MDF_2p             **mdf_x, **mdf_y;
  double           *cal_a, *cal_b, *cal_c;
  // SMILE_tool_box [HA_Calc_s2] Flag_Modify_PMT_Gain / Modified_Parameter_Data_Dir:
  // when set, cal_c (the 662 keV peak position used to re-normalize gain)
  // is overridden per-pixel from Mean_Sigma_HA%02d_137Cs_662keV.dat instead
  // of being derived from cal_a/cal_b alone.
  bool              modify_pmt_gain;
  bool              emu_mode;          // simulated data: renumber PMT / pixel IDs
  std::string       ModifiedPrmDir;
  // SMILE_tool_box [HA_Calc_s2] SingleHit_Eth(keV) / SingleHit_ADCth(ADC):
  // a single-hit event is only valid if the hit pixel's calibrated energy
  // exceeds Eth AND no OTHER pixel's raw ADC sum exceeds ADCth (purity cut).
  double            SingleHit_Eth;
  double            SingleHit_ADCth;
  double           *pos_x, *pos_y, *pos_z;
  double           *dir_xx, *dir_xy, *dir_yx, *dir_yy, *dir_zx, *dir_zy;  
  
  const std::vector<int>* ha_id;
  const ReadData_2p*         RawData = nullptr;
  const HA_CenterGravity_2p* HA_Grv = nullptr;
  std::vector<double>     det_eng;
  std::vector<double>     det_x;
  std::vector<double>     det_y;
  std::vector<double>     det_z;
  short                   hit_flag_num_ = 0;
  short                   ha_pixel_[8];
  double                  ha_grv_[2] = {0, 0}, ha_mdf_[2] = {0, 0};

  bool              ReadPrm4MDF();
  bool              ReadCalPrm();
};

#endif
