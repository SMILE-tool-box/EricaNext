#include "TPC_Calibration.h"

ANLStatus TPC_Calibration::mod_define(){
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

  if (!RawData->Get_Track_A() || !RawData->Get_Track_C())
    return AS_QUIT;

  int Nbin = RawData->Get_Track_A()->GetXaxis()->GetNbins();
  max_anode = RawData->Get_Track_A()->GetXaxis()->GetBinUpEdge(Nbin+1);
  Nbin = RawData->Get_Track_C()->GetXaxis()->GetNbins();
  max_cathode = RawData->Get_Track_C()->GetXaxis()->GetBinUpEdge(Nbin+1);
  Nbin = RawData->Get_Track_C()->GetYaxis()->GetNbins();
  max_clock = RawData->Get_Track_A()->GetYaxis()->GetBinUpEdge(Nbin+1);
  Na = Q_TPC->Get_Nadc_A();
  Nc = Q_TPC->Get_Nadc_C();
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
      cal_dat >> cal_a[na*Nc+nc] >> cal_b[na*Nc+nc];
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

  //20260605 小野田修正
   for(int clk=min_d; clk<=max_d; clk++){
    for(int ai=min_a; ai<=max_a; ai++){
      char hit_a = RawData->Get_Track_A()->GetBinContent(ai+1, clk+1);
      int a_adc = (ai) / (max_anode/Na);
      int div_a = (ai) / 384;
      if(hit_a){
	for(int cj=min_c; cj<=max_c; cj++){
	  char hit_c = RawData->Get_Track_C()->GetBinContent(cj+1, clk+1);
	  int c_adc = (cj) / (max_cathode/Nc);
    int div_c = (cj) / 384;

	  if(hit_a & hit_c & (div_a == div_c)){
	    if(Coinci_TH){ Coinci_A->Fill(ai, clk); Coinci_C->Fill(cj, clk); }
	    if(Image_XY_TH) Image_XY->Fill(ai, cj);
	    HitPanel.at(a_adc*Nc+c_adc) += 1;
      // short tx, ty;
      // if(div_a ==0){
      //   tx = 384+ai%384; ty = cj%384;
      // }
      // else if(div_a ==1){
      //   tx = 767-cj%384; ty = 384+ai%384;
      // }
      // else if(div_a ==2){
      //   tx = 383-ai%384; ty = 767-cj%384;
      // }
      // else {
      //   tx = cj%384; ty = 383-ai%384;
      // }
   
	  }
	}
      }
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
    if(no_cal_data)  Energy = +charge;
    }
  }
  if(Spectrum_TH) Spectrum->Fill(Energy);


  return AS_OK;
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
