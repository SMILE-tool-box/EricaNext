#ifndef ReadData_h
#define ReadData_h 1

#include <anlnext/BasicModule.hh>
#include <sstream>
#include <filesystem>
#include <vector>

#include <TH1.h>
#include <TH2.h>
#include <TTree.h>
#include <TCanvas.h>
#include "TError.h"
#include <TStyle.h>
#include <TGraph.h>

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
  const TH2C*              Get_Track_A()    const {return Track_A;};
  const TH2C*              Get_Track_C()    const {return Track_C;};
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
  bool              UseNtime;
  bool              SaveTrack;
  bool              SaveFADC;
  bool              SaveFADC_A_filter;
  int               FADC_A_filter_min;
  int               FADC_A_filter_max;
  bool              SaveFADC_C_filter;
  int               FADC_C_filter_min;
  int               FADC_C_filter_max;

  ANLStatus         ScanFiles();
  bool              FileExists(std::string);
  
  std::vector<int>  N_anode, N_cathode, N_HA2p;
  std::vector<unsigned> Event_ID;
  
  TPC_Encoder     **Anode, **Cathode;
  TH2D             *Image_A, **FADC_Wave_A, **FADC_ev_A;
  TH2D             *Image_C, **FADC_Wave_C, **FADC_ev_C;
  TH2C             *Track_A, *Track_C;
  TCanvas          *c_track = nullptr;
  TGraph           *FADC_C_filter_graph = nullptr;
  TGraph           *FADC_A_filter_graph = nullptr;
  int               min_a, max_a, min_c, max_c;
  int               min_ad, max_ad, min_cd, max_cd;
  void              Scan_Encoder();
  void              Read_Anode();
  void              Read_Cathode();

  CP80256         **SM2p_HA;
  TH2D            **HA2p_OK, **HA2p_NG;
  void              Scan_CP80256();
  void              Read_CP80256();
};

#endif
