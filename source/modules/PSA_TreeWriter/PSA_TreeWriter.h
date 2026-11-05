#ifndef PSA_TreeWriter_h
#define PSA_TreeWriter_h 1

#include <anlnext/BasicModule.hh>
#include <vector>

#include <TTree.h>

#include "SaveFile.h"
#include "PSA_ReadData.h"
#include "PSA_CalcFADC.h"
#include "PSA_CenterGravity.h"
#include "PSA_Calibration.h"

using namespace anlnext;

// Writes the PSA hits to a TTree (PSA_TreeWriter/psa), one row per hit channel,
// the same quantities as the .h5 of psa_decode.py (+ pixel_id.py when
// PSA_Calibration is in the chain). Chain it after the PSA_* modules.
class PSA_TreeWriter : public BasicModule {
  DEFINE_ANL_MODULE(PSA_TreeWriter, 1.0);

 public:
  PSA_TreeWriter();
  ~PSA_TreeWriter();

  ANLStatus mod_define()     override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze()    override;

 private:
  bool save_waveform_;

  const PSA_ReadData*      rd_   = nullptr;
  const PSA_CalcFADC*      fadc_ = nullptr;
  const PSA_CenterGravity* cg_   = nullptr;
  const PSA_Calibration*   cal_  = nullptr;
  TTree* tree_ = nullptr;

  unsigned sevt_id_, trig_, clk_;
  int      epoch_;
  double   time_;
  short    board_, ch_, hitnum_, peak_id_;
  bool     satu_h_, satu_l_;
  double   hg_, lg_, x_hg_, y_hg_, x_lg_, y_lg_;
  float    wave_sum_[8], ped_[8], temp_[6];
  float    temp_mean_;
  double   x_corr_, y_corr_, energy_;
  short    pix_n_, pix_m_, pixel_;
  unsigned short adc_[PSA_Board::N_SAMPLE * PSA_Board::N_SIG];
};

#endif
