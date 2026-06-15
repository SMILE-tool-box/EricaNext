#include "ReadData.h"

ANLStatus ReadData::ScanFiles(){
  ////////////////////////////////////////////////
  // Gigabit Iwaki board [ver 1, 3.1]
  Scan_Encoder();
  
  ////////////////////////////////////////////////
  // ClearPulse HA80256 [SMILE-2+]
  if(!TPC_Only){
    Scan_CP80256();
  }

  ////////////////////////////////////////////////
  if(N_anode.empty() && N_cathode.empty())
    return AS_QUIT;

  if(!TPC_Only && N_HA2p.empty())
    return AS_QUIT;
  
  return AS_OK;
}

bool ReadData::FileExists(std::string filename){
  std::string fname = DataDir + "/" + filename;
  bool flag = std::filesystem::exists(fname);

  return flag;
}
