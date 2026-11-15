#ifndef RawTreeWriter_2p_h
#define RawTreeWriter_2p_h 1

#include <anlnext/BasicModule.hh>
#include <vector>

#include <TTree.h>

#include "SaveFile.h"
#include "ReadData_2p.h"

using namespace anlnext;

// Writes the event-synchronized raw data of ReadData_2p to a TTree
// (RawTreeWriter_2p/raw), one row per event, so that calibrations (gain maps,
// TPC energy calibration, drift velocity) can be redone without re-reading
// the raw files:
//   - every TPC (strip, clock) hit bit, anode and cathode
//   - full 512-sample FADC waveforms of every channel of the boards that
//     have data in this event (optionally only channels whose maximum
//     excursion above the baseline exceeds `save_threshold`; 0 = off)
//   - HA ADC values (masked 12 bit) and error bits of every module
//   - board header time words (anode) and event IDs
// Chain it right after ReadData_2p.
class RawTreeWriter_2p : public BasicModule {
  DEFINE_ANL_MODULE(RawTreeWriter_2p, 1.0);

 public:
  RawTreeWriter_2p();
  ~RawTreeWriter_2p();

  ANLStatus mod_define()     override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze()    override;

 private:
  const ReadData_2p* RawData = nullptr;

  // parameters
  double save_threshold;   // [ADC] above the baseline
  int    base_window;      // samples used for the baseline in the save decision
  bool   save_fadc;
  bool   save_hits;   // hit_*_strip/clock and rec_*_board/clock
  bool   save_ha;

  int n_anode = 0, n_cathode = 0, n_ha = 0;

  TTree* tree_ = nullptr;
  TTree* info_ = nullptr;

  // per-event branches
  unsigned event_id, sevt_id;
  int      loopback;
  std::vector<unsigned> anode_time, cathode_time;   // per board (100 us counter)
  std::vector<short> hit_a_strip, hit_a_clock, hit_c_strip, hit_c_clock;
  // every encoder hit record (board, clock), also those whose strip bits are all
  // zero -- what SMILE_tool_box RawRead2p fills into Drift_V_Anode/Cathode
  std::vector<short> rec_a_board, rec_a_clock, rec_c_board, rec_c_clock;
  // fadc_ch: 0-11 anode (4*board + ch), 12-23 cathode; fadc_wave holds 512
  // samples per entry of fadc_ch, in the same order. Cathode waveforms are
  // stored inverted (1023 - ADC) exactly as CalcFADC_2p receives them.
  std::vector<short> fadc_ch, fadc_wave;
  std::vector<short> ha_adc;      // n_ha * 24
  std::vector<char>  ha_err;      // n_ha * 24

  // run info
  std::vector<int> anode_ids, cathode_ids, ha_ids;
};

#endif
