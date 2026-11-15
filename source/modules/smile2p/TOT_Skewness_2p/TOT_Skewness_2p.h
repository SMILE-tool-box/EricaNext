#ifndef TOT_Skewness_2p_h
#define TOT_Skewness_2p_h 1

#include <anlnext/BasicModule.hh>
#include <vector>

#include <TH1.h>
#include <TH2.h>
#include <TTree.h>

#include "SaveFile.h"
#include "ReadData_2p.h"

using namespace anlnext;

// Ported from SMILE_tool_box_v2.20/Erica/TOT_Skewness_Analysis
// (single-board / single-quadrant SMILE-2+ TPC: 384 anode strips, 384
// cathode strips, one ASIC board set -- no quadrant splitting).
class TOT_Skewness_2p : public BasicModule {
  DEFINE_ANL_MODULE(TOT_Skewness_2p, 1.0);

 public:
  TOT_Skewness_2p();
  ~TOT_Skewness_2p();

  ANLStatus mod_define()     override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze()    override;
  ANLStatus mod_finalize()   override;

  const double* GetPe()        const { return Pe; }
  const double* GetVe()        const { return Ve; }
  double        GetLen()       const { return Len; }
  double        GetSkewnessX() const { return skewness_x; }
  double        GetSkewnessY() const { return skewness_y; }
  const double* GetTrackMin()  const { return Track_Min; }
  const double* GetTrackMax()  const { return Track_Max; }
  const int*    GetFitStatus() const { return fit_status; }
  const double* GetGradErr()   const { return grad_err; }

 private:
  const ReadData_2p* RawData = nullptr;

  // Parameters
  double      Pitch_X;
  double      Pitch_Y;
  std::string DriftVelocityFile;   // TPC_Calibration_2p/.../drift_velocity.dat
  bool        Flag_Pixel_Cut;
  bool        Flag_Save_Cloud;

  // Derived geometry
  double TPC_Pitch[3];
  double TPC_Clock_Offset;
  // SMILE_tool_box: drift pitch [mm/clock] vs run time [hour], a TH1F
  // (float contents) built from the drift_velocity.dat points.
  TH1F*  h_velocity = nullptr;
  int    TPC_STRIP_ALL;
  int    TPC_DEPTH;

  // Per-event results
  double Pe[3], Ve[3], Len;
  double skewness_x, skewness_y;
  double Track_Min[3], Track_Max[3];
  int    fit_status[2];   // SMILE_tool_box s2t Ve_TRA_fit_status
  double grad_err[2];     // SMILE_tool_box s2t Ve_TRA_grad_err
  int    NUM[2];

  // SMILE_tool_box TOT_Correction(): per-strip TOT-corrected "clock" value
  // used for the 3D (anode x cathode x clock) coincidence search.
  std::vector<int>    str_a,   str_c;
  std::vector<double> corr_tot_a, corr_tot_c;

  // SMILE_tool_box TOTs_ax/TOTs_az/TOTs_cx/TOTs_cz: mm-converted position
  // of *every* raw hit strip (not TOT-corrected, not coincidence-matched).
  // Used by CalcSkewness() only; the direction (Ve) fit uses the
  // TOT-corrected per-strip points (str_a/corr_tot_a, str_c/corr_tot_c).
  std::vector<double> TOTs_ax, TOTs_az, TOTs_cx, TOTs_cz;

  // SMILE_tool_box caq0/caq1/caq2: 3D-coincidence-matched hit cloud, mm.
  std::vector<double> caq0, caq1, caq2;
  int can;

  // Output objects
  TTree* tree_            = nullptr;

  void   ClearEvent();
  void   TOT_Correction();
  void   CalcSkewness();
  int    Komtan_3D_Coincidence(std::vector<double>* acp0,
                                std::vector<double>* acp1,
                                std::vector<double>* acp2);
  int    ConvertCloudPosition(int n,
           std::vector<double>& acp0, std::vector<double>& acp1, std::vector<double>& acp2,
           std::vector<double>& out0,  std::vector<double>& out1,  std::vector<double>& out2);
  void   ConvertCloudPosition(std::vector<int>& x, std::vector<double>& xz,
                               std::vector<int>& y, std::vector<double>& yz,
                               std::vector<double>& cx, std::vector<double>& cxz,
                               std::vector<double>& cy, std::vector<double>& cyz);
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
