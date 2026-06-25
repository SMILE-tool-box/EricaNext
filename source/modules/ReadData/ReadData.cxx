#include "ReadData.h"

ANLStatus ReadData::mod_define(){
  define_parameter("data_dir", &mod_class::DataDir);
  define_parameter("reverse_TPC", &mod_class::Reverse);
  define_parameter("TPC_Only", &mod_class::TPC_Only);
  define_parameter("event_id", &mod_class::UseNtime);
  define_parameter("save_track_png", &mod_class::SaveTrack);
  define_parameter("save_fadc_png", &mod_class::SaveFADC);
  define_parameter("save_fadc_a_filter", &mod_class::SaveFADC_A_filter);
  define_parameter("fadc_a_filter_min",  &mod_class::FADC_A_filter_min);
  define_parameter("fadc_a_filter_max",  &mod_class::FADC_A_filter_max);
  define_parameter("save_fadc_c_filter", &mod_class::SaveFADC_C_filter);
  define_parameter("fadc_c_filter_min",  &mod_class::FADC_C_filter_min);
  define_parameter("fadc_c_filter_max",  &mod_class::FADC_C_filter_max);
  define_parameter("Image_A_C",          &mod_class::Image_A_C);
  define_parameter("FADC_TH",            &mod_class::FADC_TH);

  return AS_OK;
}

ANLStatus ReadData::mod_initialize(){
  if(exist_module("SaveFile")){
    SaveFile* saveModule = nullptr;
    get_module_NC("SaveFile", &saveModule);
    saveModule->cd();
    saveModule->GetDirectory()->mkdir("ReadData");
    saveModule->GetDirectory()->cd("ReadData");
  }
  ANLStatus flag = ScanFiles();
  define_evs("IDcheck_Loopback");
  if(SaveTrack || SaveFADC || SaveFADC_A_filter || SaveFADC_C_filter){
    gErrorIgnoreLevel = kWarning;
    c_track = new TCanvas("c_track", "Track", 800, 600);
    if(SaveTrack){
      std::filesystem::create_directories("./png_A");
      std::filesystem::create_directories("./png_C");
    }
    if(SaveFADC){
      std::filesystem::create_directories("./png_FADC_A");
      std::filesystem::create_directories("./png_FADC_C");
    }
    if(SaveFADC_A_filter){
      std::filesystem::create_directories("./png_FADC_A_filter");
    }
    if(SaveFADC_C_filter){
      std::filesystem::create_directories("./png_FADC_C_filter");
    }
  }


  return flag;
}

ANLStatus ReadData::mod_begin_run(){
  int flag=0;

  for(unsigned i=0; i<N_anode.size(); i++){
    flag |= Anode[i]->Init();
    Event_ID.push_back(0);
  }
  for(unsigned i=0; i<N_cathode.size(); i++){
    flag |= Cathode[i]->Init();
    Event_ID.push_back(0);
  }
  if(!TPC_Only){
  for(unsigned i=0; i<N_HA2p.size(); i++){
      flag |= SM2p_HA[i]->Init();
      Event_ID.push_back(0);
    }
  }

  if(flag)  return AS_ERROR;
  return AS_OK;
}

ANLStatus ReadData::mod_analyze(){
  ////////////////////////////////////////////////
  // ID check
  int loop_flag=0;
  unsigned max_id=0;
  do{
    int offset=0;

    if(is_evs_defined("Anode_Data")){
      for(unsigned i=0; i<N_anode.size(); i++){
	if(!loop_flag || (max_id!=Event_ID[i+offset])){
	  unsigned id;
	  int flag = Anode[i]->GetEventID(id);
	  if(flag)
	    return AS_QUIT_ALL;
	  Event_ID[i+offset] = id;
	  if(id > max_id)  max_id = id;
	}
      }
      offset += N_anode.size();
    }

    if(is_evs_defined("Cathode_Data")){
      for(unsigned i=0; i<N_cathode.size(); i++){
	if(!loop_flag || (max_id!=Event_ID[i+offset])){
	  unsigned id;
	  int flag = Cathode[i]->GetEventID(id);
	  if(flag)
	    return AS_QUIT_ALL;
	  Event_ID[i+offset] = id;
	  if(id > max_id)  max_id = id;
	}
      }
      offset += N_cathode.size();
    }

    if(is_evs_defined("ClearPulse80256")){
      for(unsigned i=0; i<N_HA2p.size(); i++){
	if(!loop_flag || (max_id!=Event_ID[i+offset])){
	  unsigned id;
	  int flag = SM2p_HA[i]->GetEventID(id);
	  if(flag)
	    return AS_QUIT_ALL;
	  Event_ID[i+offset] = id;
	  if(id > max_id)  max_id = id;
	}
      }
      offset += N_HA2p.size();
    }
  
    
    loop_flag = 1;
    for(unsigned i=0; i<Event_ID.size(); i++)
      if(Event_ID[i] != max_id){
	loop_flag = -1;
	set_evs("IDcheck_Loopback");
      }
  }while(loop_flag!=1);
  event_id_coincidence = max_id;
  
  ////////////////////////////////////////////////
  // Read Data...
  if(is_evs_defined("Anode_Data")){
    Read_Anode();
    set_evs("Anode_Data");
    if(SaveTrack){
      Int_t red_pal[2] = {kWhite, kRed};
      gStyle->SetPalette(2, red_pal);
      c_track->SetCanvasSize(800, 600);
      c_track->Clear();
      Track_A->Draw("col");
      char fname[256];
      sprintf(fname, "./png_A/Track_A_%08u.png", max_id);
      c_track->SaveAs(fname);
    }
    if(SaveFADC){
      Int_t red_pal[2] = {kWhite, kRed};
      gStyle->SetPalette(2, red_pal);
      c_track->SetCanvasSize(2000, 500);
      char fname[256];
      for(unsigned i=0; i<N_anode.size(); i++){
        c_track->Clear();
        c_track->Divide(4, 1);
        for(int ch=0; ch<4; ch++){
          c_track->cd(ch+1);
          FADC_ev_A[i*4+ch]->Draw("col");
        }
        sprintf(fname, "./png_FADC_A/FADC_A%d_%08u.png", N_anode[i], max_id);
        c_track->SaveAs(fname);
      }
    }

    if(SaveFADC_A_filter){
      int n_ch_hit = 0;
      unsigned ntime = 0;
      for(unsigned i=0; i<N_anode.size(); i++){
        if(!Anode[i]->TrackSize()) continue;
        if(ntime == 0) ntime = Anode[i]->GetNtime();
        for(int ch=0; ch<4; ch++){
          for(int clk=0; clk<512; clk++){
            short val = Anode[i]->FADC_data(ch, clk);
            if(val <= FADC_A_filter_min || val >= FADC_A_filter_max){
              n_ch_hit++;
              break;
            }
          }
        }
      }
      if(n_ch_hit > 0){
        if(FADC_A_filter_graph)
          FADC_A_filter_graph->SetPoint(FADC_A_filter_graph->GetN(),
                                        ntime, n_ch_hit);
        Int_t red_pal[2] = {kWhite, kRed};
        gStyle->SetPalette(2, red_pal);
        c_track->SetCanvasSize(2000, 500);
        char fname[256];
        for(unsigned i=0; i<N_anode.size(); i++){
          if(!Anode[i]->TrackSize()) continue;
          c_track->Clear();
          c_track->Divide(4, 1);
          for(int ch=0; ch<4; ch++){
            c_track->cd(ch+1);
            FADC_ev_A[i*4+ch]->Draw("col");
          }
          sprintf(fname, "./png_FADC_A_filter/FADC_A%d_%08u.png", N_anode[i], max_id);
          c_track->SaveAs(fname);
        }
      }
    }

  }
  if(is_evs_defined("Cathode_Data")){
    Read_Cathode();
    set_evs("Cathode_Data");
    if(SaveTrack){
      Int_t blue_pal[2] = {kWhite, kBlue};
      gStyle->SetPalette(2, blue_pal);
      c_track->SetCanvasSize(800, 600);
      c_track->Clear();
      Track_C->Draw("col");
      char fname[256];
      sprintf(fname, "./png_C/Track_C_%08u.png", max_id);
      c_track->SaveAs(fname);
    }
    if(SaveFADC){
      Int_t blue_pal[2] = {kWhite, kBlue};
      gStyle->SetPalette(2, blue_pal);
      c_track->SetCanvasSize(2000, 500);
      char fname[256];
      for(unsigned i=0; i<N_cathode.size(); i++){
        c_track->Clear();
        c_track->Divide(4, 1);
        for(int ch=0; ch<4; ch++){
          c_track->cd(ch+1);
          FADC_ev_C[i*4+ch]->Draw("col");
        }
        sprintf(fname, "./png_FADC_C/FADC_C%d_%08u.png", N_cathode[i], max_id);
        c_track->SaveAs(fname);
      }
    }
    if(SaveFADC_C_filter){
      int n_ch_hit = 0;
      unsigned ntime = 0;
      for(unsigned i=0; i<N_cathode.size(); i++){
        if(!Cathode[i]->TrackSize()) continue;
        if(ntime == 0) ntime = Cathode[i]->GetNtime();
        for(int ch=0; ch<4; ch++){
          for(int clk=0; clk<512; clk++){
            short val = Cathode[i]->FADC_data(ch, clk);
            if(val <= FADC_C_filter_min || val >= FADC_C_filter_max){
              n_ch_hit++;
              break;
            }
          }
        }
      }
      if(n_ch_hit > 0){
        if(FADC_C_filter_graph)
          FADC_C_filter_graph->SetPoint(FADC_C_filter_graph->GetN(),
                                        ntime, n_ch_hit);
        Int_t blue_pal[2] = {kWhite, kBlue};
        gStyle->SetPalette(2, blue_pal);
        c_track->SetCanvasSize(2000, 500);
        char fname[256];
        for(unsigned i=0; i<N_cathode.size(); i++){
          if(!Cathode[i]->TrackSize()) continue;
          c_track->Clear();
          c_track->Divide(4, 1);
          for(int ch=0; ch<4; ch++){
            c_track->cd(ch+1);
            FADC_ev_C[i*4+ch]->Draw("col");
          }
          sprintf(fname, "./png_FADC_C_filter/FADC_C%d_%08u.png", N_cathode[i], max_id);
          c_track->SaveAs(fname);
        }
      }
    }
  }
  if(is_evs_defined("ClearPulse80256")){
    Read_CP80256();
    set_evs("ClearPulse80256");
  }

  return AS_OK;
}

ANLStatus ReadData::mod_finalize(){
  if(SaveTrack || SaveFADC || SaveFADC_A_filter || SaveFADC_C_filter) delete c_track;
  if(is_evs_defined("ClearPulse80256")){
    for(unsigned i=0; i<N_HA2p.size(); i++){
      delete SM2p_HA[i];
    }
    delete [] SM2p_HA;
  }

  return AS_OK;
}


ReadData::ReadData(){
  DataDir = "./";
  Reverse = 0;
  TPC_Only = false;
  UseNtime = false;
  SaveTrack = false;
  SaveFADC = false;
  SaveFADC_A_filter = false;
  FADC_A_filter_min = 0;
  FADC_A_filter_max = 1023;
  SaveFADC_C_filter = false;
  FADC_C_filter_min = 0;
  FADC_C_filter_max = 1023;
  Image_A_C = false;
  FADC_TH = false;
  Image_A = nullptr;
  Image_C = nullptr;
  FADC_Wave_A = nullptr;  FADC_ev_A = nullptr;
  FADC_Wave_C = nullptr;  FADC_ev_C = nullptr;
}

ReadData::~ReadData(){}


