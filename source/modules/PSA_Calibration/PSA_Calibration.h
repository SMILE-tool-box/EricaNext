#ifndef PSA_Calibration_h
#define PSA_Calibration_h 1

#include <anlnext/BasicModule.hh>
#include <string>
#include <vector>

#include <TH1.h>
#include <TH2.h>

#include "SaveFile.h"
#include "ThinPlateSpline.h"
#include "PSA_ReadData.h"
#include "PSA_CalcFADC.h"
#include "PSA_CenterGravity.h"

using namespace anlnext;

// Position and energy of the PSA hits.
//
// Position: the 64 peaks of the x_hg-y_hg map of every ch (position_haNN.csv of
// peak_search.py, columns ch,No,x,y,z, in position_dir) are given pixel numbers
// (n, m) = 1-8 as grid_spect.py does (sorted by x into 8 columns of 8, each column
// sorted by y), unless the file has columns n,m. A thin-plate spline through them
// maps (x_hg, y_hg) to (x_corr, y_corr), where pixel (n, m) is centered at integers;
//   n - 0.5 <= x_corr < n + 0.5, m - 0.5 <= y_corr < m + 0.5,
// outside 8x8: pix_n = pix_m = -1 (same as pixel_id.py).
// pixel = 64 ch + 8 (n - 1) + (m - 1), 0-383, -1 outside.
//
// Energy (optional, energy_haNN.csv in energy_dir, columns ch,n,m,p0,p1):
//   hg_t = hg / (1 + temp_coef (T - temp_ref)),  T = mean of the 6 temperatures of the board
//   energy = p0 + p1 hg_t   [keV]
// -1 when the board or pixel has no energy calibration or the hit is outside 8x8.
// x_corr, y_corr are NaN (pixel -1) for a ch without position calibration.
class PSA_Calibration : public BasicModule {
  DEFINE_ANL_MODULE(PSA_Calibration, 1.0);

 public:
  PSA_Calibration();
  ~PSA_Calibration();

  ANLStatus mod_define()     override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze()    override;

  static constexpr int N_CH   = PSA_Board::N_CH;
  static constexpr int N_GRID = 8;

  double XCorr(int i, int ch)  const { return x_corr_[i][ch]; }
  double YCorr(int i, int ch)  const { return y_corr_[i][ch]; }
  int    PixN(int i, int ch)   const { return pix_n_[i][ch]; }
  int    PixM(int i, int ch)   const { return pix_m_[i][ch]; }
  int    Pixel(int i, int ch)  const { return pix_n_[i][ch] < 0 ? -1 : 64*ch + 8*(pix_n_[i][ch]-1) + (pix_m_[i][ch]-1); }
  double Energy(int i, int ch) const { return energy_[i][ch]; }
  bool   HasPosition(int i, int ch) const { return tps_[i][ch].Ready(); }
  bool   HasEnergy(int i)       const { return has_energy_[i]; }

 private:
  bool LoadPosition(int i, const std::string& path);
  bool LoadEnergy(int i, const std::string& path);

  // parameters
  std::string position_dir_, energy_dir_;
  double temp_coef_, temp_ref_;
  bool   save_maps_;
  double hg_max_;
  int    hg_bins_;
  double energy_max_;
  int    energy_bins_;

  const PSA_ReadData*      rd_   = nullptr;
  const PSA_CalcFADC*      fadc_ = nullptr;
  const PSA_CenterGravity* cg_   = nullptr;
  int nb_ = 0;
  std::vector<std::vector<ThinPlateSpline>> tps_;
  std::vector<std::vector<double>> x_corr_, y_corr_, energy_;
  std::vector<std::vector<int>> pix_n_, pix_m_;
  std::vector<bool> has_energy_;
  // per board: p0, p1 of pixel 0-383 (NaN = no calibration)
  std::vector<std::vector<double>> p0_, p1_;
  std::vector<std::vector<TH2D*>> corr_maps_;
  std::vector<TH2D*> pixel_spec_;
  std::vector<TH1D*> energy_spec_;
};

#endif
