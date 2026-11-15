#include "ReadData_2p.h"

ANLStatus ReadData_2p::ScanFiles(){
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

// A board that was not read out leaves a first file smaller than one event (0 bytes, or a
// fragment such as 20 bytes in the TPC-only runs of 2017). One TPC encoder event is at least
// 16 (header) + 4096 (FADC) + 4 (version) + 4 (footer) = 4120 bytes.
bool ReadData_2p::FileEmpty(std::string filename){
  return std::filesystem::file_size(DataDir + "/" + filename) < 4120;
}

bool ReadData_2p::FileExists(std::string filename){
  std::string fname = DataDir + "/" + filename;
  bool flag = std::filesystem::exists(fname);

  return flag;
}
