#include "TPC_Calibration.h"
#include <algorithm>

ANLStatus TPC_Calibration::mod_define(){
  define_parameter("energy_correction_file", &mod_class::energy_correction_file_);
  define_parameter("ParameterFile", &mod_class::PrmFile);
  define_parameter("NoCalData",     &mod_class::no_cal_data);
  define_parameter("Coinci_TH",     &mod_class::Coinci_TH);
  define_parameter("Image_XY_TH",   &mod_class::Image_XY_TH);
  define_parameter("Spectrum_TH",   &mod_class::Spectrum_TH);
  return AS_OK;
}

ANLStatus TPC_Calibration::mod_initialize(){
  if(!(exist_module("CalcFADC")))
    return AS_QUIT;
  get_module("CalcFADC", &Q_TPC);
  get_module("ReadData", &RawData);
  
  if(exist_module("SaveFile")){
    SaveFile* saveModule = nullptr;
    get_module_NC("SaveFile", &saveModule);
    saveModule->cd();
    saveModule->GetDirectory()->mkdir("TPC_Calibration");
    saveModule->GetDirectory()->cd("TPC_Calibration");

  }

  if (RawData->Get_AnodeID()->empty() || RawData->Get_CathodeID()->empty())
    return AS_QUIT;

  // Same ranges as ReadData's Track_A/C (1536 strips x 1024 clocks). The coincidences are counted from the hit
  // lists, so the Track_A/C histograms are not requested here (they cost a 3 MB reset and fill per event).
  max_anode = kStrips;
  max_cathode = kStrips;
  max_clock = kClockBins;
  // FADC channel = 4 board + ch = strip / 32 (0-47), the index of CalcFADC's Get_Qanode/Get_Qcathode, also
  // when boards are missing (until 2026-11-06 the channel was strip / (1536 / (4 x boards present)), wrong
  // and out of range when a board was missing, e.g. c01 of run_20260623T072649Z)
  Na = 4 * 12;
  Nc = 4 * 12;
  if(Coinci_TH){
    Coinci_A = new TH2C("Coinci_A", "Coincidence_A",
			max_anode, -0.5, max_anode-0.5,
			max_clock, -0.5, max_clock-0.5);
    Coinci_C = new TH2C("Coinci_C", "Coincidence_C",
			max_cathode, -0.5, max_cathode-0.5,
			max_clock, -0.5, max_clock-0.5);
  }
  if(Image_XY_TH){
    Image_XY = new TH2D("Image_XY", "Image_XY",
			max_anode, -0.5, max_anode-0.5,
			max_cathode, -0.5, max_cathode-0.5);
    Image_XY->SetXTitle("Anode");
    Image_XY->SetYTitle("Cathode");
  }
  if(Spectrum_TH){
    Spectrum = new TH1D("Spectrum", "TPC_Spec", 2000, 0, 400);
    Spectrum->SetXTitle("Energy [keV]");
  }
  define_evs("TPC_Ee_Valid");
  
  HitPanel.resize(Na*Nc);
  cal_a.resize(Na*Nc);
  cal_b.resize(Na*Nc);
  if(!no_cal_data){
    std::ifstream cal_dat(PrmFile.c_str());
    if(!cal_dat)  return AS_QUIT;
    while(cal_dat){
      int na, nc;
      cal_dat >> na >> nc;
      if(cal_dat.eof())  break;
      double ka, kb;
      cal_dat >> ka >> kb;
      if(na < 0 || na >= Na || nc < 0 || nc >= Nc)  continue;
      cal_a[na*Nc+nc] = ka;
      cal_b[na*Nc+nc] = kb;
    }
  }
  
  if(!energy_correction_file_.empty()){
    if(no_cal_data){
      std::cerr << "TPC_Calibration: energy correction requires NoCalData=false" << std::endl;
      return AS_CRITICAL_ERROR_TO_FINALIZE;
    }
    try {energy_correction_.Load(energy_correction_file_);}
    catch(const std::exception& e){std::cerr << e.what() << std::endl;return AS_CRITICAL_ERROR_TO_FINALIZE;}
    if(Spectrum_TH){
      Spectrum_Gain = new TH1D("Spectrum_GainMap", "Before global correction;Energy [keV];events",2000,0,400);
    }
  }
  return AS_OK;
}

ANLStatus TPC_Calibration::mod_analyze(){
  if(Coinci_TH){ Coinci_A->Reset("ICES"); Coinci_C->Reset("ICES"); }
  std::fill(HitPanel.begin(), HitPanel.end(), 0);
  int min_a = RawData->GetMinAnode();
  int max_a = RawData->GetMaxAnode();
  int min_c = RawData->GetMinCathode();
  int max_c = RawData->GetMaxCathode();
  int min_d = RawData->GetMinClock();
  int max_d = RawData->GetMaxClock();
  
  // for(int clk=min_d; clk<max_d; clk++){
  //   for(int ai=min_a; ai<max_a; ai++){
  //     char hit_a = RawData->Get_Track_A()->GetBinContent(ai, clk);
  //     int a_adc = (ai-1) / (max_anode/Na);
  //     if(hit_a){
	// for(int cj=min_c; cj<max_c; cj++){
	//   char hit_c = RawData->Get_Track_C()->GetBinContent(cj, clk);	
	//   int c_adc = (cj-1) / (max_cathode/Nc);

	//   if(hit_a & hit_c){
	//     Coinci_A->Fill(ai-1, clk-1);
	//     Coinci_C->Fill(cj-1, clk-1);
	//     Image_XY->Fill(ai-1, cj-1);
	//     HitPanel.at(a_adc*Nc+c_adc) += 1;

  //20260605 小野田修正 (anode/cathode coincidence in the same clock and the same panel of 384 strips)
  // Until 2026-11-17 this scanned every (clock, strip) of the bounding box of Track_A/C with GetBinContent,
  // which was most of the CPU time of the chain. Now the occupied cells are built from the hit lists and only
  // they are paired; the result is identical to the scan, including the TH2C details it relied on:
  //  - a cell holds the number of hits in it, saturated at 127 (TH2C);
  //  - the condition was hit_a & hit_c & (div_a == div_c), bitwise with the bool as 1, i.e. both counts odd
  //    (a cell with 2 hits never coincides);
  //  - clocks >= 1024 all fall into the overflow row, which the scan read once for each clock in
  //    [max(min_d, 1024), max_d], so a coincidence there counts that many times.
  if(max_d >= 0){
    BuildCells(RawData->Get_Anode_Hits(), cells_a_);
    BuildCells(RawData->Get_Cathode_Hits(), cells_c_);
    const int n_overflow = (max_d >= kClockBins) ? max_d - std::max(min_d, kClockBins) + 1 : 0;
    size_t ia = 0, ic = 0;
    while(ia < cells_a_.size() && ic < cells_c_.size()){
      const int row = cells_a_[ia].row;
      if(cells_c_[ic].row < row){ ++ic; continue; }
      size_t ea = ia;
      while(ea < cells_a_.size() && cells_a_[ea].row == row) ++ea;
      if(cells_c_[ic].row > row){ ia = ea; continue; }
      size_t ec = ic;
      while(ec < cells_c_.size() && cells_c_[ec].row == row) ++ec;
      const int repeat = (row < kClockBins) ? 1 : n_overflow;
      for(size_t a = ia; a < ea; ++a){
        const int ai = cells_a_[a].strip;
        const int a_adc = ai / 32;   // FADC channel 4 board + ch
        const int div_a = ai / 384;
        for(size_t c = ic; c < ec; ++c){
          const int cj = cells_c_[c].strip;
          if(cj / 384 != div_a || !(cells_a_[a].count & cells_c_[c].count & 1)) continue;
          HitPanel.at(a_adc*Nc + cj/32) += repeat;
          for(int k = 0; k < repeat; ++k){
            const int clk = (row < kClockBins) ? row : std::max(min_d, kClockBins) + k;
            if(Coinci_TH){ Coinci_A->Fill(ai, clk); Coinci_C->Fill(cj, clk); }
            if(Image_XY_TH) Image_XY->Fill(ai, cj);
          }
        }
      }
      ia = ea; ic = ec;
    }
  }

  Energy = 0;
  for(int i=0; i<Na; i++){
    double total=0;
    for(int j=0; j<Nc; j++)
      total += (double)(HitPanel.at(i*Nc+j));

    if(total>0){
      double charge = Q_TPC->Get_Qanode(i);
      for(int j=0; j<Nc; j++)
	Energy += (charge * cal_a[i*Nc+j] + cal_b[i*Nc+j])
	  * ((double)(HitPanel.at(i*Nc+j))/total);
    if(no_cal_data)  Energy += charge;   // sum of the anode channels with coincidences
    }
  }
  if(energy_correction_.enabled && Energy>0){
    if(Spectrum_Gain)Spectrum_Gain->Fill(Energy);
    Energy=energy_correction_.Apply(Energy);
  }
  // SMILE_tool_box TPC_Calibration::Process(): evs "Ee" only when Ee != 0; this module never skips,
  // Reconstruct checks TPC_Ee_Valid
  if(Energy != 0)  set_evs("TPC_Ee_Valid");
  if(Spectrum_TH && Energy != 0) Spectrum->Fill(Energy);


  return AS_OK;
}

// Occupied cells of Track_A/C as (row, strip, count), sorted by row then strip: row = min(clock, 1024) (the
// overflow row), count saturated at 127 like TH2C.
void TPC_Calibration::BuildCells(const std::vector<ReadData::Hit>& hits, std::vector<Cell>& cells){
  cell_keys_.clear();
  for(const auto& h : hits){
    const int row = std::min<int>(h.clock, kClockBins);
    cell_keys_.push_back(row*kStrips + h.strip);
  }
  std::sort(cell_keys_.begin(), cell_keys_.end());
  cells.clear();
  for(size_t i = 0; i < cell_keys_.size();){
    size_t j = i;
    while(j < cell_keys_.size() && cell_keys_[j] == cell_keys_[i]) ++j;
    cells.push_back({cell_keys_[i] / kStrips, cell_keys_[i] % kStrips, (int)std::min<size_t>(j - i, 127)});
    i = j;
  }
}

ANLStatus TPC_Calibration::mod_finalize(){
  return AS_OK;
}


TPC_Calibration::TPC_Calibration(){
  PrmFile = "./Calibration.dat";
  Coinci_TH   = false;
  Image_XY_TH = false;
  Spectrum_TH = false;
}

TPC_Calibration::~TPC_Calibration(){
}
