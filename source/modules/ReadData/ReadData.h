#ifndef ReadData_h
#define ReadData_h 1

#include <anlnext/BasicModule.hh>
#include <sstream>
#include <filesystem>
#include <vector>

#include <TH2.h>

#include "SaveFile.h"
#include "TPC_Encoder.h"
#include "CP80256.h"

using namespace anlnext;

class ReadData : public BasicModule {
  DEFINE_ANL_MODULE(ReadData, 1.0);

public:
  ReadData();
  ~ReadData();

  ANLStatus mod_define() override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_begin_run() override;
  ANLStatus mod_analyze() override;
  ANLStatus mod_finalize() override; 

  int       Get_Npmt_HA2p()  const {return 6;};
  int       Get_Nch_HA2p()   const {return 4;};
  void      Get_ADC_HA2p(int ha, short *data)  const;
  const std::vector<int>*  Get_HA2pID()  const {return &N_HA2p;};

  void      Get_TPC_FADC(bool c_flag, int id, short *data) const;
  int       Get_FADC_clk()  const {return 512;};
  const std::vector<int>*  Get_AnodeID()    const {return &N_anode;};
  const std::vector<int>*  Get_CathodeID()  const {return &N_cathode;};
  // Allocate per-event histograms only when a reconstruction consumer requests them.
  const TH2C*              Get_Track_A() const;
  const TH2C*              Get_Track_C() const;

  struct Hit { short strip; short clock; };
  const std::vector<Hit>& Get_Anode_Hits()   const { return anode_hits_; }
  const std::vector<Hit>& Get_Cathode_Hits() const { return cathode_hits_; }
  int                GetMinAnode()    const {return min_a;};
  int                GetMaxAnode()    const {return max_a;};
  int                GetMinCathode()  const {return min_c;};
  int                GetMaxCathode()  const {return max_c;};
  unsigned           Get_EventID_coincidence()    const {return event_id_coincidence;};
  // 0-based count of the merged (board-synchronized) events processed so far
  unsigned           Get_SEvtID()     const {return sevt_id_;};
  // header time word N_time of anode / cathode board i (index in Get_AnodeID() / Get_CathodeID())
  unsigned           Get_AnodeTime(int i)   const {return Anode[i]->GetNtime();};
  unsigned           Get_CathodeTime(int i) const {return Cathode[i]->GetNtime();};
  // every hit record of the encoder (board number, clock), also records whose strip bits are all zero
  struct Record { short board; short clock; };
  const std::vector<Record>& Get_Anode_Records()   const { return anode_records_; }
  const std::vector<Record>& Get_Cathode_Records() const { return cathode_records_; }
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
  // other directories with encoder files of the same run (the other storage node, e.g. killua for the
  // cathode boards when data_dir is calltt). Each board is read from the first directory that has its
  // first file (data_dir, then extra_data_dirs in order).
  std::vector<std::string> ExtraDataDirs;
  std::vector<std::string> DirAnode, DirCathode;   // directory of each board in N_anode / N_cathode
  bool              Reverse;
  bool              TPC_Only;
  bool              UseNtime;
  unsigned          event_id_coincidence;

  ANLStatus         ScanFiles();
  bool              FileExists(std::string);
  std::string       FindDir(const std::string&);   // first of data_dir, extra_data_dirs with the file; "" if none
  
  std::vector<int>  N_anode, N_cathode, N_HA2p;
  std::vector<unsigned> Event_ID;
  unsigned          event_id = 0;
  
  TPC_Encoder     **Anode, **Cathode;
  // Per-event working histograms for reconstruction; never written to the output file.
  mutable TH2C     *Track_A, *Track_C;
  int               min_a, max_a, min_c, max_c;
  int               min_ad, max_ad, min_cd, max_cd;
  std::vector<Hit>  anode_hits_, cathode_hits_;
  std::vector<Record> anode_records_, cathode_records_;
  unsigned          sevt_id_ = 0, next_sevt_id_ = 0;
  void              Scan_Encoder();
  void              Read_Anode();
  void              Read_Cathode();

  CP80256         **SM2p_HA;
  TH2D            **HA2p_OK, **HA2p_NG;
  void              Scan_CP80256();
  void              Read_CP80256();
};

#endif
