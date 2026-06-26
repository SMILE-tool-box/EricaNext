#include "CalcFADC.h"

ANLStatus CalcFADC::mod_define(){
  define_parameter("base_window", &mod_class::base_window);
  define_parameter("threshold", &mod_class::threshold);
  define_parameter("save_TH_plots", &mod_class::Save_TH_plots);

  return AS_OK;
}

ANLStatus CalcFADC::mod_initialize(){
  if(!(exist_module("ReadData")))
    return AS_QUIT;
  get_module("ReadData", &RawData);
  Q_anode   = new double [4*12];
  Q_cathode = new double [4*12];

  if(exist_module("SaveFile") && Save_TH_plots){
    SaveFile* saveModule = nullptr;
    get_module_NC("SaveFile", &saveModule);
    saveModule->cd();
    saveModule->GetDirectory()->mkdir("CalcFADC");
    saveModule->GetDirectory()->cd("CalcFADC");

    unsigned N_a = RawData->Get_AnodeID()->size();
    unsigned N_c = RawData->Get_CathodeID()->size();

    BaseAnode = new TH2D("BaselineAnode", "Baseline Anode",
			 256, -0.5, 255.5, 4*12, -0.5, 4*12-0.5);
    BaseAnode->SetXTitle("[ADU]");
    BaseAnode->SetYTitle("[ch]");

    ChargeAnode = new TH2D("ChargeAnode", "Charge Anode",
			   2000, 0, 1e4, 4*12, -0.5, 4*12-0.5);
    ChargeAnode->SetXTitle("[ADU]");
    ChargeAnode->SetYTitle("[ch]");

    BaseCathode = new TH2D("BaselineCathode", "Baseline Cathode",
			   256, -0.5, 255.5, 4*12, -0.5, 4*12-0.5);
    BaseCathode->SetXTitle("[ADU]");
    BaseCathode->SetYTitle("[ch]");

    ChargeCathode = new TH2D("ChargeCathode", "Charge Cathode",
			     2000, 0, 1e4, 4*12, -0.5, 4*12-0.5);
    ChargeCathode->SetXTitle("[ADU]");
    ChargeCathode->SetYTitle("[ch]");

    // 4panels * 12ch(a) * 12ch(c) = 576, nullptr初期化
    Spec1Hit_A = new TH1D* [576]();
    Spec1Hit_C = new TH1D* [576]();
    for(int i=0; i<(int)(4*N_a); i++){
      int phys_a = (*RawData->Get_AnodeID())[i/4];
      int panel_a = phys_a / 3;
      int local_a = (phys_a%3)*4 + (i%4);
      for(int j=0; j<(int)(4*N_c); j++){
        int phys_c = (*RawData->Get_CathodeID())[j/4];
        if(phys_c/3 != panel_a) continue;
        int local_c = (phys_c%3)*4 + (j%4);
        int area = panel_a*144 + local_a*12 + local_c;
        char histname[100];
        sprintf(histname, "Anode_1Hit_A%02d_C%02d", phys_a*4+(i%4), phys_c*4+(j%4));
        Spec1Hit_A[area] = new TH1D(histname, histname, 2000, 0, 1e4);
        sprintf(histname, "Cathode_1Hit_A%02d_C%02d", phys_a*4+(i%4), phys_c*4+(j%4));
        Spec1Hit_C[area] = new TH1D(histname, histname, 2000, 0, 1e4);
      }
    }
  }

  define_evs("FADC_NoHit");

  return AS_OK;
}

ANLStatus CalcFADC::mod_analyze(){
  unsigned N_a = RawData->Get_AnodeID()->size();
  unsigned N_c = RawData->Get_CathodeID()->size();

  std::fill(Q_anode,   Q_anode   + 4*12, 0.0);
  std::fill(Q_cathode, Q_cathode + 4*12, 0.0);

  std::vector<std::thread> threads;
  threads.push_back(std::thread(&CalcFADC::IntegWave, this, 0, N_a));
  threads.push_back(std::thread(&CalcFADC::IntegWave, this, 1, N_c));
  for(std::thread &t: threads)
    t.join();

  int hit_A=0, hit_C=0, A_ID=-1, C_ID=-1;
  for(int i=0; i<(int)N_a; i++){
    int phys_a = (*RawData->Get_AnodeID())[i];
    for(int ch=0; ch<4; ch++){
      int idx = 4*phys_a+ch;
      if(Q_anode[idx] > 0){ hit_A++; A_ID = idx; }
    }
  }

  if(hit_A == 1){
    int panel_a = (A_ID/4) / 3;
    for(int i=0; i<(int)N_c; i++){
      int phys_c = (*RawData->Get_CathodeID())[i];
      if(phys_c/3 == panel_a){
        for(int ch=0; ch<4; ch++){
          int idx = 4*phys_c+ch;
          if(Q_cathode[idx] > 0){ hit_C++; C_ID = idx; }
        }
      }
    }
  }

  if(hit_A==1 && hit_C==1 && Save_TH_plots){
    int phys_a = A_ID/4;
    int phys_c = C_ID/4;
    int panel_a = phys_a / 3;
    int local_a = (phys_a%3)*4 + (A_ID%4);
    int local_c = (phys_c%3)*4 + (C_ID%4);
    unsigned area = panel_a*144 + local_a*12 + local_c;
    Spec1Hit_A[area]->Fill(Q_anode[A_ID]);
    Spec1Hit_C[area]->Fill(Q_cathode[C_ID]);
  }

  if(A_ID<0 || C_ID<0){
    set_evs("FADC_NoHit");
    return AS_SKIP;
  }

  return AS_OK;
}

void CalcFADC::IntegWave(bool c_flag, unsigned Nid){
  int Nclk = (RawData->Get_FADC_clk());
  short *wave = new short [4*Nclk];

  for(unsigned id=0; id<Nid; id++){
    RawData->Get_TPC_FADC(c_flag, id, wave);

    for(int ch=0; ch<4; ch++){
      double mean=0, charge=0;

      for(int clk=0; clk<base_window; clk++)
	mean += wave[ch*Nclk+clk]/base_window;
      for(int clk=TMath::FloorNint(base_window); clk<Nclk; clk++){
	if(wave[ch*Nclk+clk] - mean > threshold){
	  charge += wave[ch*Nclk+clk] - mean;
	}
      }

      if(c_flag){
        int phys_c = (*RawData->Get_CathodeID())[id];
	Q_cathode[4*phys_c+ch] = charge;
	if(exist_module("SaveFile") && Save_TH_plots && charge > 0){
	  BaseCathode->Fill(mean, 4*phys_c+ch);
	  ChargeCathode->Fill(charge, 4*phys_c+ch);
	}
      }
      else{
        int phys_a = (*RawData->Get_AnodeID())[id];
	Q_anode[4*phys_a+ch] = charge;
	if(exist_module("SaveFile") && Save_TH_plots && charge > 0){
	  BaseAnode->Fill(mean, 4*phys_a+ch);
	  ChargeAnode->Fill(charge, 4*phys_a+ch);
	}
      }
    }
  }

  delete [] wave;
}

ANLStatus CalcFADC::mod_finalize(){
  delete [] Q_anode;  delete [] Q_cathode;

  return AS_OK;
}


CalcFADC::CalcFADC(){
  base_window = 30;
  threshold = 20;
}

CalcFADC::~CalcFADC(){
}
