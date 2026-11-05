#ifndef PSA_CalcFADC_h
#define PSA_CalcFADC_h 1

#include <anlnext/BasicModule.hh>
#include <string>
#include <vector>

#include <TH2.h>

#include "SaveFile.h"
#include "PSA_ReadData.h"

using namespace anlnext;

// Waveform integration of the PSA (MPPC) signals, as psa_decode.py:
//   - pedestal: mean of the first `pedestal_samples` samples, per signal (HG0-3, LG0-3)
//   - peak sample: the maximum of the pedestal-subtracted LG sum (first one when equal)
//   - integral (trapezoid) from peak - window_front to peak + window_back,
//     or from fixed_start to fixed_stop when fixed_window = true
//   - hg = sum of the 4 HG integrals, lg = sum of the 4 LG integrals
//   - temperatures [degC] = (raw - b) / a, a and b from psa_param_file (row HA = ha<NN>)
//     in the same order as psa_decode.py (TL..BR of the csv reversed onto the 6 words)
// Values are computed for the channels with a hit bit of the present boards.
// Index i is the board index of PSA_ReadData.
class PSA_CalcFADC : public BasicModule {
  DEFINE_ANL_MODULE(PSA_CalcFADC, 1.0);

 public:
  PSA_CalcFADC();
  ~PSA_CalcFADC();

  ANLStatus mod_define()     override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze()    override;

  static constexpr int N_CH  = PSA_Board::N_CH;
  static constexpr int N_SIG = PSA_Board::N_SIG;

  bool   Hit(int i, int ch)          const { return hit_[i][ch]; }
  double HG(int i, int ch)           const { return sum_[i][ch][0] + sum_[i][ch][1] + sum_[i][ch][2] + sum_[i][ch][3]; }
  double LG(int i, int ch)           const { return sum_[i][ch][4] + sum_[i][ch][5] + sum_[i][ch][6] + sum_[i][ch][7]; }
  // integral of signal sig (0-3 HG of the 4 corners, 4-7 LG) [ADC x sample]
  double Sum(int i, int ch, int sig) const { return sum_[i][ch][sig]; }
  double Ped(int i, int ch, int sig) const { return ped_[i][ch][sig]; }
  int    PeakID(int i, int ch)       const { return peak_[i][ch]; }
  bool   SatuH(int i, int ch)        const { return rd_->Board(i).SatuH(ch); }
  bool   SatuL(int i, int ch)        const { return rd_->Board(i).SatuL(ch); }
  double Temp(int i, int k)          const { return temp_[i][k]; }
  double TempMean(int i)             const { return temp_mean_[i]; }
  bool   TempCalibrated(int i)       const { return temp_cal_[i]; }

 private:
  bool LoadTempParam(int id, double* a, double* b) const;

  // parameters
  int    pedestal_samples_;
  int    window_front_, window_back_;
  bool   fixed_window_;
  int    fixed_start_, fixed_stop_;
  std::string psa_param_file_;
  bool   save_spectra_;
  double hg_max_, lg_max_;
  int    spec_bins_;

  const PSA_ReadData* rd_ = nullptr;
  int nb_ = 0;
  std::vector<std::vector<bool>> hit_;
  std::vector<std::vector<std::vector<double>>> sum_, ped_;
  std::vector<std::vector<int>> peak_;
  std::vector<std::vector<double>> temp_, temp_a_, temp_b_;
  std::vector<double> temp_mean_;
  std::vector<bool> temp_cal_;
  std::vector<TH2D*> spec_hg_, spec_lg_;
};

#endif
