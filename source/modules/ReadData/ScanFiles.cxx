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

std::string ReadData::FindDir(const std::string& filename){
  std::vector<std::string> dirs{DataDir};
  dirs.insert(dirs.end(), ExtraDataDirs.begin(), ExtraDataDirs.end());
  std::string found;
  for(const auto& d : dirs){
    if(!std::filesystem::exists(d + "/" + filename))  continue;
    if(found.empty())  found = d;
    else  std::cerr << "ReadData: WARNING: " << filename << " is in " << found << " and " << d
                    << "; reading " << found << std::endl;
  }
  return found;
}

bool ReadData::FileExists(std::string filename){
  std::string fname = DataDir + "/" + filename;
  bool flag = std::filesystem::exists(fname);

  return flag;
}
