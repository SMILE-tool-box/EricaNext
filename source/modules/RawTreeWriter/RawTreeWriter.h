#ifndef RawTreeWriter_h
#define RawTreeWriter_h 1

#include <anlnext/BasicModule.hh>
#include <vector>

#include <TTree.h>

#include "SaveFile.h"
#include "ReadData.h"
#include "PSA_ReadData.h"
#include "PSA_RawEvent.h"

using namespace anlnext;

// Saves the raw information of the readers present in the chain.
// Existing TPC_RawTreeWriter/* and PSA_RawTreeWriter/* on-disk paths are retained
// so that existing reconstruction and analysis scripts can read both old and new files.
// Place this module after ReadData and/or PSA_ReadData, before any event selection.
class RawTreeWriter : public BasicModule {
  DEFINE_ANL_MODULE(RawTreeWriter, 1.0);

 public:
  RawTreeWriter();
  ~RawTreeWriter();

  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze()    override;

  static constexpr int N_BOARD = 12;   // boards per side (anode or cathode)

 private:
  ANLStatus InitializeTPC();
  ANLStatus AnalyzeTPC();
  ANLStatus InitializePSA();
  ANLStatus AnalyzePSA();
  const PSA_ReadData* psa_reader_ = nullptr;
  TTree* psa_tree_ = nullptr;
  std::vector<int> psa_ids_;
  PSA_RawEvent psa_row_;
  int psa_schema_version_=2, psa_n_sample_=50, psa_n_sig_=8;
  const ReadData* RawData = nullptr;

  int tpc_schema_version_ = 2;

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
