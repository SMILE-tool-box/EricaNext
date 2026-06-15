#include "TPC_Encoder.h"

int TPC_Encoder::GetEventID(unsigned& eid){
  int flag = 0;
  unsigned header = 0;
  track.clear();
  do{
    header = ReadData(flag, header);
    if(flag)
      return 2;
  }while(header!=0xeb901964);
  unsigned N_trg = ReadData(flag);
  unsigned N_time = ReadData(flag);
  unsigned N_gso = ReadData(flag);

  unsigned check = ReadData(flag);
  if(check != 0x75504943){
    raw.seekg(-4, std::ios::cur);
    for(int i=0; i<512; i++){
      unsigned fadc0 = ReadData(flag);
      unsigned fadc1 = ReadData(flag);

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

    unsigned tmp = ReadData(flag);
    for(int i=0; i<1024; i++){
      unsigned clk = ReadData(flag);
      if(clk == 0x75504943){
	break;
      }

      unsigned trk[4] = {0, 0, 0, 0};
      for(int ch=0; ch<4; ch++){
	unsigned raw_trk = ReadData(flag);

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
   m_N_time = N_time;
  
  if(ntime_flag)
    //eid = N_time;
    eid = N_trg ;
  else
    eid = N_gso & 0x1fffffff;
  return 0;
}

std::string TPC_Encoder::FileName(bool next_flag=0){
  if(next_flag)
    N_file++;
  char num[10];
  sprintf(num, "%05d", N_file);
  
  std::string filename = dir + "/tpc_enc_";
  if(ac_flag)   filename += "c";
  else          filename += "a";
  char id_str[10];
  sprintf(id_str, "%02d", id);
  filename += std::string(id_str) + "_" + num + ".raw";

  return filename;
}

int TPC_Encoder::Init(){
  raw.open(FileName());
  if(raw)
    std::cout << FileName() << std::endl;

  bool flag=0;
  int check=0;
  unsigned header=0;
  do{
    header = ReadData(check, header);
    if(check)  break;
    
    if(header == 0xeb901964)
      flag = 1;
  }while(!flag);
  raw.seekg(-4, std::ios::cur);

  return check;
}

unsigned TPC_Encoder::ReadData(int& flag){
  flag = 0;
  char data[4];
  for(int i=0; i<4; i++){
    raw.read(&data[i], 1);
    if(raw.eof()){
      raw.close();
      raw.open(FileName(1));
      if(raw)
	std::cout << FileName() << std::endl;
      else{
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

unsigned TPC_Encoder::ReadData(int& flag, unsigned initial){
  flag = 0;

  char data;
  raw.read(&data, 1);
  if(raw.eof()){
    raw.close();
    raw.open(FileName(1));
    if(raw)
      std::cout << FileName() << std::endl;
    else
      flag = 2;
    raw.read(&data, 1);
  }

  unsigned val = (initial & 0xffffff) << 8 | (data & 0xff);
  return val;
}


TPC_Encoder::TPC_Encoder(std::string d_name, bool flag, int b_id, bool reverse, bool ntime){
  dir = d_name;
  ac_flag = flag;
  id = b_id;
  N_file = 0;
  rev_flag = reverse;
  ntime_flag = ntime;
}

TPC_Encoder::~TPC_Encoder(){
}
