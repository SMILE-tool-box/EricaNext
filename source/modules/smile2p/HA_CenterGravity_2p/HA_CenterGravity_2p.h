#ifndef HA_CenterGravity_2p_h
#define HA_CenterGravity_2p_h 1

#include <anlnext/BasicModule.hh>
#include <TH2.h>
#include <mutex>
#include <thread>

#include "SaveFile.h"
#include "ReadData_2p.h"

using namespace anlnext;

class HA_CenterGravity_2p : public BasicModule {
  DEFINE_ANL_MODULE(HA_CenterGravity_2p, 1.0);

 public:
  HA_CenterGravity_2p();
  ~HA_CenterGravity_2p();

  ANLStatus mod_define() override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze() override;
  ANLStatus mod_finalize() override;
  
  void      CG_Process(int);
  const std::vector<int>* Hit_HA()  const {return &hit_ID;};
  void                    Hit_Info(int id, int &ha, int &pmt,
				   double &e, double &x, double &y) const;
  // SMILE_tool_box HA_Center_Gravity::Process(): HA_RAW_E, summed from the
  // masked ADC value regardless of the per-channel error bit or threshold.
  // Used by HA_Calc_2p for the Hit_E_Num purity check (SMILE_tool_box
  // HA_Calc_s2::Count_Hit_Num()/Set_HA_Pixel()).
  double    Get_RawE(int id)  const {return raw_e[id];};
  int       Get_NHA()  const {return N_HA;};
  int       Get_NPMT() const {return N_PMT;};

 private:
  short             threshold, overflow;
  int               N_HA, N_PMT, N_ADC;
  std::vector<int>  hit_ID;
  const std::vector<int>* ha_ID;
  double           *gr_x, *gr_y, *gr_e, *raw_e;
  std::mutex        key;
  
  const ReadData_2p* RawData = nullptr;
};

#endif
