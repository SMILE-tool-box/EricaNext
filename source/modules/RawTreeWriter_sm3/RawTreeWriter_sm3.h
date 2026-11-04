#ifndef RawTreeWriter_sm3_h
#define RawTreeWriter_sm3_h 1

#include <anlnext/BasicModule.hh>
#include <vector>

#include <TTree.h>

#include "SaveFile.h"
#include "ReadData.h"

using namespace anlnext;

// Writes the event-synchronized TPC raw data of ReadData to a TTree
// (RawTreeWriter_sm3/raw), one row per event, so that calibrations (gain maps,
// energy calibration, drift velocity) can be redone without re-reading the
// raw files. SMILE-3: 12 anode and 12 cathode boards (4 uPIC panels x 3),
// some of which may be missing in a run.
//   - every TPC (strip, clock) hit bit, anode and cathode
//     (strip = 128*board + 32*ch + bit, 0-1535; panel = strip/384)
//   - every encoder hit record (board, clock)
//   - full 512-sample FADC waveforms of every channel of the boards that
//     have data in this event (optionally only channels whose maximum
//     excursion above the baseline exceeds `save_threshold`; 0 = off)
//   - header time words and event IDs
// The HA (PSA, MPPC waveforms) is not read by ReadData yet and not written.
// Chain it right after ReadData.
class RawTreeWriter_sm3 : public BasicModule {
  DEFINE_ANL_MODULE(RawTreeWriter_sm3, 1.0);

 public:
  RawTreeWriter_sm3();
  ~RawTreeWriter_sm3();

  ANLStatus mod_define()     override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze()    override;

  static constexpr int N_BOARD = 12;   // boards per side (anode or cathode)

 private:
  const ReadData* RawData = nullptr;

  // parameters
  double save_threshold;   // [ADU] above the baseline
  int    base_window;      // [samples] used for the baseline in the save decision
  bool   save_fadc;
  bool   save_hits;        // hit_*_strip/clock and rec_*_board/clock

  int n_anode = 0, n_cathode = 0;

  TTree* tree_ = nullptr;
  TTree* info_ = nullptr;

  // per-event branches
  unsigned event_id, sevt_id;
  int      loopback;
  std::vector<unsigned> anode_time, cathode_time;   // per board, in the order of anode_ids / cathode_ids
  std::vector<short> hit_a_strip, hit_a_clock, hit_c_strip, hit_c_clock;
  std::vector<short> rec_a_board, rec_a_clock, rec_c_board, rec_c_clock;
  // fadc_ch: 0-47 anode (4*board + ch), 48-95 cathode (48 + 4*board + ch), with the
  // board number of the file name (tpc_enc_a<board>), so a missing board leaves a gap.
  // fadc_wave holds 512 samples per entry of fadc_ch, in the same order. Cathode
  // waveforms are stored inverted (1023 - ADC) exactly as CalcFADC receives them.
  std::vector<short> fadc_ch, fadc_wave;

  // run info
  std::vector<int> anode_ids, cathode_ids;
  int n_fadc_clk = 0;
};

#endif
