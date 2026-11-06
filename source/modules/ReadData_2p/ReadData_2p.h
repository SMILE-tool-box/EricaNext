#ifndef ReadData_2p_h
#define ReadData_2p_h 1

#include <anlnext/BasicModule.hh>
#include <sstream>
#include <filesystem>
#include <vector>

#include <TH1.h>
#include <TH2.h>
#include <TTree.h>

#include "SaveFile.h"
#include "TPC_Encoder_2p.h"
#include "CP80256_2p.h"

using namespace anlnext;

class ReadData_2p : public BasicModule {
  DEFINE_ANL_MODULE(ReadData_2p, 1.0);

public:
  ReadData_2p();
  ~ReadData_2p();

  ANLStatus mod_define() override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_begin_run() override;
  ANLStatus mod_analyze() override;
  ANLStatus mod_finalize() override; 

  int       Get_Npmt_HA2p()  const {return 6;};
  int       Get_Nch_HA2p()   const {return 4;};
  void      Get_ADC_HA2p(int ha, short *data)  const;
  // SMILE_tool_box HA_Center_Gravity::Process(): the masked 12-bit ADC
  // value is kept and summed into HA_RAW_E even when the hardware error
  // bit is set (the error bit only ever affects HA_Hit_Flag, not HA_RAW_E).
  // Get_ADC_HA2p() above instead replaces errored channels with -1, which
  // loses that raw value; this variant preserves it and reports the error
  // flags (same 24-entry butterfly-offset order) separately.
  void      Get_ADC_HA2p_Raw(int ha, short *data, bool *err) const;
  const std::vector<int>*  Get_HA2pID()  const {return &N_HA2p;};

  void      Get_TPC_FADC(bool c_flag, int id, short *data) const;
  int       Get_FADC_clk()  const {return 512;};
  const std::vector<int>*  Get_AnodeID()    const {return &N_anode;};
  const std::vector<int>*  Get_CathodeID()  const {return &N_cathode;};
  const TH2C*              Get_Track_A()    const {return Track_A;};
  const TH2C*              Get_Track_C()    const {return Track_C;};
  // SMILE_tool_box RawRead2p: the synchronized hardware trigger counter
  // shared by all boards for this merged event (same quantity as the
  // reference's anode_id[]), usable as a cross-run comparison key.
  unsigned                 Get_EventID()    const {return event_id_;};
  // SMILE_tool_box RawRead2p: SEvt_ID, a plain 0-based count of merged
  // events processed so far (no duplicates possible, unlike the hardware
  // trigger counter above) -- the most reliable cross-run join key.
  unsigned                 Get_SEvtID()     const {return sevt_id_;};
  // SMILE_tool_box RawRead2p "Anode_Time": header time word of anode board
  // `i` (units of 0.1 ms; TOT_Skewness_Analysis uses anode_time[0]/1e4/3600
  // [hour] to look up the time-dependent drift velocity).
  unsigned                 Get_AnodeTime(int i) const {return Anode[i]->EventTime();};
  // SMILE_tool_box TPC_Calibration_2p "TOT_Area" (nhit[2]): number of
  // (strip, clock) hit bits in this event, anode / cathode.
  double                   Get_NHitA()      const {return n_hit_a_;};
  double                   Get_NHitC()      const {return n_hit_c_;};
  // every (strip, clock) hit bit of this event, in decoding order
  const std::vector<short>& Get_HitStripA() const {return hit_strip_a_;};
  const std::vector<short>& Get_HitClockA() const {return hit_clk_a_;};
  const std::vector<short>& Get_HitStripC() const {return hit_strip_c_;};
  const std::vector<short>& Get_HitClockC() const {return hit_clk_c_;};
  // every hit record of the encoder (one per board and clock, also records whose
  // strip bits are all zero), as SMILE_tool_box RawRead2p fills Drift_V_Anode/Cathode
  const std::vector<short>& Get_RecordBoardA() const {return rec_board_a_;};
  const std::vector<short>& Get_RecordClockA() const {return rec_clk_a_;};
  const std::vector<short>& Get_RecordBoardC() const {return rec_board_c_;};
  const std::vector<short>& Get_RecordClockC() const {return rec_clk_c_;};
  unsigned                 Get_CathodeTime(int i) const {return Cathode[i]->EventTime();};
  int                GetMinAnode()    const {return min_a;};
  int                GetMaxAnode()    const {return max_a;};
  int                GetMinCathode()  const {return min_c;};
  int                GetMaxCathode()  const {return max_c;};
  int                GetMinClock()    const {
    int val = (min_ad > min_cd) ? min_cd : min_ad;
    return val;
  };
  int                GetMaxClock()    const {
    int val = (max_ad < max_cd) ? max_cd : max_ad;
    return val;
  };
  
 private:
  std::string       DataDir;
  bool              Reverse;
  bool              TPC_Only;

  ANLStatus         ScanFiles();
  bool              FileExists(std::string);
  bool      FileEmpty(std::string);
  
  std::vector<int>  N_anode, N_cathode, N_HA2p;
  std::vector<unsigned> Event_ID;
  unsigned          event_id_ = 0;
  unsigned          sevt_id_ = 0;
  unsigned          next_sevt_id_ = 0;
  double            n_hit_a_ = 0, n_hit_c_ = 0;
  std::vector<short> rec_board_a_, rec_clk_a_, rec_board_c_, rec_clk_c_;
  std::vector<short> hit_strip_a_, hit_clk_a_, hit_strip_c_, hit_clk_c_;
  
  TPC_Encoder_2p     **Anode, **Cathode;
  TH2C             *Track_A, *Track_C;
  int               min_a, max_a, min_c, max_c;
  int               min_ad, max_ad, min_cd, max_cd;
  void              Scan_Encoder();
  void              Read_Anode();
  void              Read_Cathode();

  CP80256_2p         **SM2p_HA;
  void              Scan_CP80256();
  void              Read_CP80256();
};

#endif
