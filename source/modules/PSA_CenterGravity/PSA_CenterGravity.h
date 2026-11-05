#ifndef PSA_CenterGravity_h
#define PSA_CenterGravity_h 1

#include <anlnext/BasicModule.hh>
#include <vector>

#include <TH2.h>

#include "SaveFile.h"
#include "PSA_ReadData.h"
#include "PSA_CalcFADC.h"

using namespace anlnext;

// Center of gravity of the 4 corner signals (Anger logic), as psa_decode.py:
//   x = (S0 - S1 - S2 + S3) / (S0 + S1 + S2 + S3)
//   y = (-S0 - S1 + S2 + S3) / (S0 + S1 + S2 + S3)
// with S = the HG integrals (x_hg, y_hg) or the LG integrals (x_lg, y_lg); 0 when the sum is 0.
// With save_maps, the x_hg-y_hg map of every board and ch is saved (binning of
// grid_spect.py), for the hits that pass map_peak_id_min and the saturation cut.
class PSA_CenterGravity : public BasicModule {
  DEFINE_ANL_MODULE(PSA_CenterGravity, 1.0);

 public:
  PSA_CenterGravity();
  ~PSA_CenterGravity();

  ANLStatus mod_define()     override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze()    override;

  static constexpr int N_CH = PSA_Board::N_CH;

  double XHG(int i, int ch) const { return x_hg_[i][ch]; }
  double YHG(int i, int ch) const { return y_hg_[i][ch]; }
  double XLG(int i, int ch) const { return x_lg_[i][ch]; }
  double YLG(int i, int ch) const { return y_lg_[i][ch]; }

 private:
  bool save_maps_;
  int  map_peak_id_min_;
  bool map_skip_saturated_;
  double map_hg_min_;

  const PSA_ReadData* rd_ = nullptr;
  const PSA_CalcFADC* fadc_ = nullptr;
  int nb_ = 0;
  std::vector<std::vector<double>> x_hg_, y_hg_, x_lg_, y_lg_;
  std::vector<std::vector<TH2D*>> maps_;
};

#endif
