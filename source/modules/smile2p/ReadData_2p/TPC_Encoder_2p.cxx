#include "TPC_Encoder_2p.h"
#include <chrono>
#include <filesystem>
#include <thread>

int TPC_Encoder_2p::GetEventID(unsigned& eid){
  int flag = 0;
  unsigned header = 0;
  track.clear();
  do{
    header = ReadData_2p(flag, header);
    if(flag)
      return 2;
  }while(header!=0xeb901964);
  ReadData_2p(flag);                 // trigger counter (unused)
  evt_time = ReadData_2p(flag);
  unsigned N_gso = ReadData_2p(flag);

  unsigned check = ReadData_2p(flag);
  if(check != 0x75504943){
    raw.seekg(-4, std::ios::cur);
    for(int i=0; i<512; i++){
      unsigned fadc0 = ReadData_2p(flag);
      unsigned fadc1 = ReadData_2p(flag);

      if(rev_flag){
	fadc[1][i] = (fadc0 >> 16) & 0x3ff;
	fadc[0][i] = fadc0 & 0x3ff;
	fadc[3][i] = (fadc1 >> 16) & 0x3ff;
	fadc[2][i] = fadc1 & 0x3ff;
      }
      else{
	fadc[0][i] = (fadc0 >> 16) & 0x3ff;
	fadc[1][i] = fadc0 & 0x3ff;
	fadc[2][i] = (fadc1 >> 16) & 0x3ff;
	fadc[3][i] = fadc1 & 0x3ff;
      }
    }

    unsigned tmp = ReadData_2p(flag);
    for(int i=0; i<1024; i++){
      unsigned clk = ReadData_2p(flag);
      if(clk == 0x75504943){
	break;
      }

      unsigned trk[4] = {0, 0, 0, 0};
      for(int ch=0; ch<4; ch++){
	unsigned raw_trk = ReadData_2p(flag);

	if(rev_flag){
	  int cr = 2 * (1-((int)(ch/2))) + (ch%2);
	  int tmp_data=0;
	  for(int bit=0; bit<32; bit++){
	    int br = 2*((int)bit/2)+(1-bit%2);
	    tmp_data |= ((raw_trk >> (bit)) & 0x1) << br;
	  }
	  trk[cr] = tmp_data;
	}
	else
	  trk[3-ch] = raw_trk;
      }
      
      track.push_back(clk);
      for(int ch=0; ch<4; ch++)
	track.push_back(trk[ch]);
    }
  }

  eid = N_gso & 0x1fffffff;
  return 0;
}

std::string TPC_Encoder_2p::FileName(bool next_flag=0){
  if(next_flag)
    N_file++;
  char num[10];
  sprintf(num, "%05d", N_file);
  
  std::string filename = dir + "/uTPC_";
  if(ac_flag)   filename += "cathode";
  else          filename += "anode";
  filename += std::to_string(id) + "_" + num + ".raw";

  return filename;
}

int TPC_Encoder_2p::Init(){
  raw.open(FileName());
  if(raw)
    std::cout << FileName() << std::endl;

  bool flag=0;
  int check=0;
  unsigned header=0;
  do{
    header = ReadData_2p(check, header);
    if(check)  break;
    
    if(header == 0xeb901964)
      flag = 1;
  }while(!flag);
  raw.seekg(-4, std::ios::cur);

  return check;
}

unsigned TPC_Encoder_2p::ReadData_2p(int& flag){
  flag = 0;
  char data[4];
  for(int i=0; i<4; i++){
    raw.read(&data[i], 1);
    if(raw.eof()){
      raw.close();
      if(!OpenNext()){
	flag = 2;
	break;
      }
      raw.read(&data[i], 1);
    }
  }

  unsigned val
    = ((data[0] & 0xff) << 24) | ((data[1] & 0xff) << 16)
    | ((data[2] & 0xff) <<  8) | (data[3] & 0xff);
  return val;
}

unsigned TPC_Encoder_2p::ReadData_2p(int& flag, unsigned initial){
  flag = 0;

  char data;
  raw.read(&data, 1);
  if(raw.eof()){
    raw.close();
    if(!OpenNext())
      flag = 2;
    raw.read(&data, 1);
  }

  unsigned val = (initial & 0xffffff) << 8 | (data & 0xff);
  return val;
}


// Open the next file of this board. A missing file is the end of the data. A file that exists
// but cannot be opened (network mounts under heavy parallel reading) is retried for up to 30 s
// instead of ending the run early.
bool TPC_Encoder_2p::OpenNext(){
  const std::string name = FileName(1);
  if(!std::filesystem::exists(name))
    return false;
  for(int retry=0; retry<30; retry++){
    raw.clear();
    raw.open(name);
    if(raw){
      std::cout << name << std::endl;
      return true;
    }
    std::cerr << "TPC_Encoder_2p: cannot open " << name << ", retrying (" << retry+1 << ")" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return false;
}

TPC_Encoder_2p::TPC_Encoder_2p(std::string d_name, bool flag, int b_id, bool reverse){
  dir = d_name;
  ac_flag = flag;
  id = b_id;
  N_file = 0;
  rev_flag = reverse;
}

TPC_Encoder_2p::~TPC_Encoder_2p(){
}
