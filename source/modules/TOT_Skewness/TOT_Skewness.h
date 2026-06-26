#ifndef TOT_skewness_h
#define TOT_skewness_h 1

#include <anlnext/BasicModule.hh>
#include <vector>

#include <TH1.h>
#include <TH2.h>
#include <TTree.h>

#include "SaveFile.h"
#include "ReadData.h"

using namespace anlnext;

class TOT_Skewness : public BasicModule {
  DEFINE_ANL_MODULE(TOT_Skewness, 1.0);

 public:
  TOT_Skewness();
  ~TOT_Skewness();

  ANLStatus mod_define()     override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze()    override;
  ANLStatus mod_finalize()   override;

  const double* GetPe()        const { return Pe; }
  const double* GetVe()        const { return Ve; }
  double        GetLen()       const { return Len; }
  double        GetSkewnessX() const { return skewness_x; }
  double        GetSkewnessY() const { return skewness_y; }

 private:
  const ReadData* RawData = nullptr;

  // Parameters
  double Pitch_X;
  double Pitch_Y;
  double Pitch_Z;
  double TPC_Clock_Offset;
  bool   Flag_Pixel_Cut;
  bool   Flag_Save_Cloud;

  // Derived geometry
  double TPC_Pitch[3];
  int    TPC_STRIP_ALL;
  int    TPC_DEPTH;

  // Per-event results
  double Pe[3], Ve[3], Len;
  double skewness_x, skewness_y;
  double Track_Min[3], Track_Max[3];
  int    NUM[2];

  std::vector<int>    str_a,   str_c;
  std::vector<double> corr_tot_a, corr_tot_c;
  std::vector<double> caq0, caq1, caq2;
  int can;

  // Output objects
  TTree* tree_            = nullptr;
  TH1F*  h_flag_pixel_cut = nullptr;
  TH2F*  h_cut_pixel      = nullptr;
  TH2F*  TPC_Map_xz       = nullptr;
  TH2F*  TPC_Map_yz       = nullptr;

  void   ClearEvent();
  void   TOT_Correction();
  void   CalcSkewness();
  int    Komtan_3D_Coincidence(std::vector<double>* acp0,
                                std::vector<double>* acp1,
                                std::vector<double>* acp2);
  int    ConvertCloudPosition(int n,
           std::vector<double>& acp0, std::vector<double>& acp1, std::vector<double>& acp2,
           std::vector<double>& out0,  std::vector<double>& out1,  std::vector<double>& out2);
  void   TPC_Track_Recon();
  void   Calc_Len(std::vector<double>& p0,
                   std::vector<double>& p1,
                   std::vector<double>& p2);
  double Get_TPC_XYZ(int xyz, int val);
  double tot_corr0(int clock_max, int clock_min);
  double tot_corr1(int clock_max, int clock_min);
  int    PixelMask(short anode, short cathode);
};

#endif
