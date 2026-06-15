#include "CalcFADC.h"

ANLStatus CalcFADC::mod_define(){
  define_parameter("base_window", &mod_class::base_window);
  define_parameter("threshold", &mod_class::threshold);

  return AS_OK;
}

ANLStatus CalcFADC::mod_initialize(){
  if(!(exist_module("ReadData")))
    return AS_QUIT;
  get_module("ReadData", &RawData);
  Q_anode = new double [4*(RawData->Get_AnodeID()->size())];
  Q_cathode = new double [4*(RawData->Get_CathodeID()->size())];
  
  if(exist_module("SaveFile")){
    SaveFile* saveModule = nullptr;
    get_module_NC("SaveFile", &saveModule);
    saveModule->cd();
    saveModule->GetDirectory()->mkdir("CalcFADC");
    saveModule->GetDirectory()->cd("CalcFADC");

    unsigned N_a = RawData->Get_AnodeID()->size();
    unsigned N_c = RawData->Get_CathodeID()->size();
    BaseAnode = new TH2D("BaselineAnode", "Baseline Anode",
			 256, -0.5, 255.5, 4*N_c, -0.5, 4*N_c-0.5);
    BaseAnode->SetXTitle("[ADU]");
    BaseAnode->SetYTitle("[ch]");
    ChargeAnode = new TH2D("ChargeAnode", "Charge Anode",
			   2000, 0, 1e4, 4*N_a, -0.5, 4*N_a-0.5);
    ChargeAnode->SetXTitle("[ADU]");
    ChargeAnode->SetYTitle("[ch]");

    BaseCathode = new TH2D("BaselineCathode", "Baseline Cathode",
			   256, -0.5, 255.5, 4*N_c, -0.5, 4*N_c-0.5);
    BaseCathode->SetXTitle("[ADU]");
    BaseCathode->SetYTitle("[ch]");

    ChargeCathode = new TH2D("ChargeCathode", "Charge Cathode",
			     2000, 0, 1e4, 4*N_c, -0.5, 4*N_c-0.5);
    ChargeCathode->SetXTitle("[ADU]");
    ChargeCathode->SetYTitle("[ch]");

    Spec1Hit_A = new TH1D* [16*N_a*N_c];
    Spec1Hit_C = new TH1D* [16*N_a*N_c];
    for(unsigned i=0; i<4*N_a; i++)
      for(unsigned j=0; j<4*N_c; j++){
	char histname[100];
	sprintf(histname, "Anode_1Hit_A%02d_C%02d", i, j);
	Spec1Hit_A[i*(4*N_c)+j] = new TH1D(histname, histname, 2000, 0, 1e4);
	sprintf(histname, "Cathode_1Hit_A%02d_C%02d", i, j);
	Spec1Hit_C[i*(4*N_c)+j] = new TH1D(histname, histname, 2000, 0, 1e4);
      }				     
  }

  define_evs("FADC_NoHit");
  
  return AS_OK;
}

ANLStatus CalcFADC::mod_analyze(){
  unsigned N_a = RawData->Get_AnodeID()->size();
  unsigned N_c = RawData->Get_CathodeID()->size();

  std::vector<std::thread> threads;
  threads.push_back(std::thread(&CalcFADC::IntegWave, this, 0, N_a));
  threads.push_back(std::thread(&CalcFADC::IntegWave, this, 1, N_c));
  for(std::thread &t: threads)
    t.join();

  int hit_A=0, hit_C=0, A_ID=-1, C_ID=-1;
  for(unsigned i=0; i<4*N_a; i++)
    if(Q_anode[i] > 0){
      hit_A++;
      A_ID = i;
    }
  for(unsigned i=0; i<4*N_c; i++)
    if(Q_cathode[i] > 0){
      hit_C++;
      C_ID = i;
    }
  if(hit_A==1 && hit_C==1){
    unsigned area = A_ID*(4*N_c) + C_ID;
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
	Q_cathode[4*id+ch] = charge;
	if(exist_module("SaveFile")&& charge > 0){//20260607 小野田修正
	  BaseCathode->Fill(mean, 4*id+ch);
	  ChargeCathode->Fill(charge, 4*id+ch);
	}
      }
      else{
	Q_anode[4*id+ch] = charge;
	if(exist_module("SaveFile")&& charge > 0){//20260607 小野田修正
	  BaseAnode->Fill(mean, 4*id+ch);
	  ChargeAnode->Fill(charge, 4*id+ch);
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
