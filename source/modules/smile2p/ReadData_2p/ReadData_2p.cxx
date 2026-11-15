#include "ReadData_2p.h"

ANLStatus ReadData_2p::mod_define(){
  define_parameter("data_dir", &mod_class::DataDir);
  define_parameter("reverse_TPC", &mod_class::Reverse);
  define_parameter("TPC_Only", &mod_class::TPC_Only);

  return AS_OK;
}

ANLStatus ReadData_2p::mod_initialize(){
  ANLStatus flag = ScanFiles();
  define_evs("IDcheck_Loopback");
  
  return flag;
}

ANLStatus ReadData_2p::mod_begin_run(){
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

ANLStatus ReadData_2p::mod_analyze(){
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
    if(!TPC_Only){
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
  }
    
    loop_flag = 1;
    for(unsigned i=0; i<Event_ID.size(); i++)
      if(Event_ID[i] != max_id){
	loop_flag = -1;
	set_evs("IDcheck_Loopback");
      }
  }while(loop_flag!=1);
  event_id_ = max_id;

  ////////////////////////////////////////////////
  // Read Data...
  if(is_evs_defined("Anode_Data")){
    Read_Anode();
    set_evs("Anode_Data");
  }
  if(is_evs_defined("Cathode_Data")){
    Read_Cathode();
    set_evs("Cathode_Data");
  }
  if(!TPC_Only){
    if(is_evs_defined("ClearPulse80256")){
      Read_CP80256();
      set_evs("ClearPulse80256");
    }
  }
  sevt_id_ = next_sevt_id_++;

  return AS_OK;
}

ANLStatus ReadData_2p::mod_finalize(){
  if(is_evs_defined("ClearPulse80256")){
    for(unsigned i=0; i<N_HA2p.size(); i++){
      delete SM2p_HA[i];
    }
    delete [] SM2p_HA;
  }

  return AS_OK;
}


ReadData_2p::ReadData_2p(){
  DataDir = "./";
  Reverse = 0;
  TPC_Only = false;
}

ReadData_2p::~ReadData_2p(){}


