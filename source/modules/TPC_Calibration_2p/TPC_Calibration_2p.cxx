#include "TPC_Calibration_2p.h"

ANLStatus TPC_Calibration_2p::mod_define(){
  define_parameter("ParameterFile", &mod_class::PrmFile);
  define_parameter("Cal_Modify",    &mod_class::Cal_Modify);

  return AS_OK;
}

ANLStatus TPC_Calibration_2p::mod_initialize(){
  if(!(exist_module("CalcFADC_2p")))
    return AS_QUIT;
  get_module("CalcFADC_2p", &Q_TPC);
  get_module("ReadData_2p", &RawData);
  
  int Nbin = RawData->Get_Track_A()->GetXaxis()->GetNbins();
  max_anode = RawData->Get_Track_A()->GetXaxis()->GetBinUpEdge(Nbin+1);
  Nbin = RawData->Get_Track_C()->GetXaxis()->GetNbins();
  max_cathode = RawData->Get_Track_C()->GetXaxis()->GetBinUpEdge(Nbin+1);
  Nbin = RawData->Get_Track_C()->GetYaxis()->GetNbins();
  max_clock = RawData->Get_Track_A()->GetYaxis()->GetBinUpEdge(Nbin+1);
  Na = Q_TPC->Get_Nadc_A();
  Nc = Q_TPC->Get_Nadc_C();
  // SMILE_tool_box: STRIP_NUM_CHIP = TPC_STRIP_ALL / (BOARD_NUM*ADC_CH)
  strip_num_chip_a = max_anode / Na;
  strip_num_chip_c = max_cathode / Nc;
  Coinci_A = new TH2C("Coinci_A", "Coincidence_A",
		      max_anode, -0.5, max_anode-0.5,
		      max_clock, -0.5, max_clock-0.5);
  Coinci_C = new TH2C("Coinci_C", "Coincidence_C",
		      max_cathode, -0.5, max_cathode-0.5,
		      max_clock, -0.5, max_clock-0.5);
  // per-event work histograms for the coincidence test below, never written
  Coinci_A->SetDirectory(nullptr);
  Coinci_C->SetDirectory(nullptr);
  
  AnodeCathodeHit.resize(max_anode*max_cathode);
  HitPanel.resize(Na*Nc);
  CountHitPanel.resize(Na);
  cal_a.resize(Na*Nc);
  cal_b.resize(Na*Nc);
  std::ifstream cal_dat(PrmFile.c_str());
  if(!cal_dat)  return AS_QUIT;
  while(cal_dat){
    int na, nc;
    cal_dat >> na >> nc;
    if(cal_dat.eof())  break;

    cal_dat >> cal_a[na*Nc+nc] >> cal_b[na*Nc+nc];
    // SMILE_tool_box TPC_Calibration_2p::Init(): cal_a/cal_b *= cal_mod
    // right after being read, i.e. before any energy calculation.
    cal_a[na*Nc+nc] *= Cal_Modify;
    cal_b[na*Nc+nc] *= Cal_Modify;
  }
  
  define_evs("No_TrackCoincidence");
  // SMILE_tool_box TPC_Calibration_2p::Process(): evs->Set("Ee") only when
  // Ee != 0 -- Reconstruct_2p checks this instead of any SKIP from this module
  // (reference's TPC_Calibration_2p never returns ANL_SKIP).
  define_evs("TPC_Ee_Valid");

  return AS_OK;
}

ANLStatus TPC_Calibration_2p::mod_analyze(){
  Coinci_A->Reset("ICES");
  Coinci_C->Reset("ICES");
  std::fill(AnodeCathodeHit.begin(), AnodeCathodeHit.end(), 0);
  std::fill(HitPanel.begin(), HitPanel.end(), 0);
  std::fill(CountHitPanel.begin(), CountHitPanel.end(), 0);
  int min_a = RawData->GetMinAnode();
  int max_a = RawData->GetMaxAnode();
  int min_c = RawData->GetMinCathode();
  int max_c = RawData->GetMaxCathode();
  int min_d = RawData->GetMinClock();
  int max_d = RawData->GetMaxClock();

  // SMILE_tool_box TPC_Calibration_2p::TPC_Coincidence(): for every clock,
  // every simultaneous (anode strip, cathode strip) pair is a coincidence;
  // tally it into the per-event anode x cathode strip count map.
  // Track_A/Track_C are value-filled (Fill(n_strip, clk)); with the default
  // -0.5..N-0.5 binning that means strip value V sits in ROOT bin V+1, so
  // GetBinContent needs (ai+1, clk+1) here while ai/clk stay raw 0-based
  // strip/clock values for Fill() and array indexing below.
  for(int clk=min_d; clk<=max_d; clk++){
    for(int ai=min_a; ai<=max_a; ai++){
      char hit_a = RawData->Get_Track_A()->GetBinContent(ai+1, clk+1);
      if(hit_a){
	for(int cj=min_c; cj<=max_c; cj++){
	  char hit_c = RawData->Get_Track_C()->GetBinContent(cj+1, clk+1);

	  if(hit_a & hit_c){
	    Coinci_A->Fill(ai, clk);
	    Coinci_C->Fill(cj, clk);
	    AnodeCathodeHit.at(ai*max_cathode+cj) += 1;
	  }
	}
      }
    }
  }
  // SMILE_tool_box TPC_Calibration_2p::Process() never skips the chain here --
  // it just lets Ee come out 0 (then turns that into the 0xffffffff
  // sentinel below) and continues. Flag it for diagnostics only.
  if(Coinci_A->GetEntries() == 0)
    set_evs("No_TrackCoincidence");

  // SMILE_tool_box TPC_Calibration_2p::Set_TPC_Panel_Hit(): drop strips within
  // 4 strips of a chip's edge (both anode and cathode independently), then
  // aggregate the surviving strip-level coincidences into ADC-panel hits.
  for(int ai=0; ai<max_anode; ai++){
    int ai_in_chip = ai % strip_num_chip_a;
    if(ai_in_chip < 4 || ai_in_chip >= strip_num_chip_a-4)  continue;
    for(int cj=0; cj<max_cathode; cj++){
      int cnt = AnodeCathodeHit.at(ai*max_cathode+cj);
      if(cnt == 0)  continue;
      int cj_in_chip = cj % strip_num_chip_c;
      if(cj_in_chip < 4 || cj_in_chip >= strip_num_chip_c-4)  continue;

      HitPanel.at((ai/strip_num_chip_a)*Nc + (cj/strip_num_chip_c)) += cnt;
    }
  }

  // SMILE_tool_box TPC_Calibration_2p::Process(): only panels with more than
  // 100 (strip x clock) coincidences contribute to the weighted energy sum.
  const int PANEL_HIT_MIN = 100;
  for(int i=0; i<Na; i++)
    for(int j=0; j<Nc; j++)
      if(HitPanel.at(i*Nc+j) > PANEL_HIT_MIN)
	CountHitPanel.at(i) += HitPanel.at(i*Nc+j);

  // SMILE_tool_box: accumulate per anode board (Ee_anode[i/ADC_CH]) first,
  // then sum the boards -- same summation order, so the result is
  // bit-identical rather than just equal to ~1e-15.
  const int ADC_CH = 4;
  std::vector<double> ee_board(Na/ADC_CH + 1, 0.);
  for(int i=0; i<Na; i++){
    if(CountHitPanel.at(i) == 0)  continue;
    double charge = Q_TPC->Get_Qanode(i);
    for(int j=0; j<Nc; j++){
      if(HitPanel.at(i*Nc+j) <= PANEL_HIT_MIN)  continue;
      ee_board[i/ADC_CH] += (charge * cal_a[i*Nc+j] + cal_b[i*Nc+j])
	* ((double)HitPanel.at(i*Nc+j) / CountHitPanel.at(i));
    }
  }
  Energy = 0;
  for(double e : ee_board)  Energy += e;

  // SMILE_tool_box TPC_Calibration_2p::Process(): if (Ee == 0) Ee = 0xffffffff;
  // else evs->Set("Ee");
  if(Energy == 0)
    Energy = (double)0xffffffffu;
  else
    set_evs("TPC_Ee_Valid");

  return AS_OK;
}

ANLStatus TPC_Calibration_2p::mod_finalize(){
  return AS_OK;
}


TPC_Calibration_2p::TPC_Calibration_2p(){
  PrmFile    = "./Calibration.dat";
  Cal_Modify = 1.0;
}

TPC_Calibration_2p::~TPC_Calibration_2p(){
}
