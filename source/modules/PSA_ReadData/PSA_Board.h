#ifndef PSA_Board_h
#define PSA_Board_h 1

#include <string>
#include <vector>

// Reader of the raw data of one SMILE-3 scintillator HA board (PSA, MPPC),
// files ha<NN>_<number>.dat of a run (a .dat.tmp is used when the .dat is
// missing, e.g. the run was stopped by a crash of the storage).
//
// Frame (big endian), as decoded by analysis_sm3_FM/PSA_analysis/src_onoda/psa_decode.py:
//   0x00  header eb 90 19 64
//   0x04  trig  (TCU trigger counter, common to all boards)
//   0x08  clk   (1 MHz)
//   0x0D  2 bytes: bits 6-11 = data count, bit (5-j) set when ADC j has data
//   0x14  3 x 32 bits: 6 temperature words (14 bits each)
//   0x24  16-byte blocks, 50 per ADC with data: ADC bit (1 << j), sample (1-50),
//         8 values of 14 bits: HG of the 4 corners, then LG of the 4 corners
//   end   footer 75 50 49 43
// Length = 40 + 800 x (ADCs with data). Channel numbering: ch = 5 - j, the "ch"
// of the psa_decode .h5 files and of position_haNN.csv of peak_search.py.
class PSA_Board {
 public:
  static constexpr int N_CH      = 6;    // scintillator blocks (8x8 pixels each) per board
  static constexpr int N_SAMPLE  = 50;   // waveform samples
  static constexpr int N_SIG     = 8;    // HG0-3, LG0-3
  static constexpr int N_TEMP    = 6;
  static constexpr int ADC_MAX   = 16383;

  PSA_Board(int id, const std::vector<std::string>& files);

  // "ha03" for board 3 (the name in the file names)
  static std::string Name(int id);

  // Read the next frame. false at the end of the data.
  bool Next();

  int      ID()      const { return id_; }
  bool     End()     const { return end_; }
  unsigned Trig()    const { return trig_; }
  unsigned Clk()     const { return clk_; }
  // incremented when trig goes back (the counter was reset, or data of the previous run)
  int      Epoch()   const { return epoch_; }
  // clk since the first frame of this board [s], wrap of the 32-bit counter unfolded
  double   Time()    const { return time_; }
  bool     Hit(int ch)     const { return (hit_mask_ >> ch) & 1; }
  int      NHit()          const;
  unsigned short TempRaw(int k) const { return temp_raw_[k]; }
  // ADC value; 0 for channels without data
  unsigned short ADC(int ch, int sample, int sig) const { return adc_[ch][sample][sig]; }
  bool     SatuH(int ch) const { return satu_h_[ch]; }
  bool     SatuL(int ch) const { return satu_l_[ch]; }

  long     NFrames()  const { return n_frames_; }
  long     NSkipped() const { return n_skipped_; }   // bytes skipped while looking for a header
  long     NResets()  const { return epoch_; }
  const std::vector<std::string>& Files() const { return files_; }

 private:
  bool LoadNextFile();
  bool FindFrame(size_t& len);
  void Decode(const unsigned char* ev, size_t len);

  int id_;
  std::vector<std::string> files_;
  size_t ifile_ = 0;
  std::vector<unsigned char> buf_;
  size_t pos_ = 0;
  bool end_ = false;

  unsigned trig_ = 0, clk_ = 0, prev_trig_ = 0, prev_clk_ = 0;
  bool first_ = true;
  int epoch_ = 0;
  double time_ = 0;
  long long clk_wraps_ = 0;
  unsigned clk0_ = 0;
  unsigned hit_mask_ = 0;   // bit ch
  unsigned short temp_raw_[N_TEMP];
  unsigned short adc_[N_CH][N_SAMPLE][N_SIG];
  bool satu_h_[N_CH], satu_l_[N_CH];
  long n_frames_ = 0, n_skipped_ = 0;
};

#endif
