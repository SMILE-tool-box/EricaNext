#include "CP80256.h"

int CP80256::GetEventID(unsigned &eid){
  int flag =0;
  unsigned short header=0;
  do{
    header = ReadData(flag, header);
    if(flag)
      return 2;
  }while((header & 0xfff0) != 0xeb90);

  char data[54];
  flag = ReadData(data);
  eid = ((data[2] & 0x1f) << 24) | ((data[3] & 0xff) << 16)
    | ((data[4] & 0xff) << 8) | (data[5] & 0xff);

  for(int i=0; i<24; i++){
    adc[i] = ((data[2*i+6] & 0xff) << 8) | (data[2*i+7] & 0xff);
  }
  
  return 0;
}

std::string CP80256::FileName(bool next_flag=0){
  if(next_flag)
    N_file++;
  char num[10], ha_id[10];
  sprintf(num, "%05d", N_file);
  sprintf(ha_id, "%02d_", id);
  
  std::string filename = dir + "/HA" + ha_id;
  filename += num;
  filename += ".raw";

  return filename;
}

int CP80256::Init(){
  raw.open(FileName());
  if(raw)
    std::cout << FileName() << std::endl;

  bool flag=0;
  int check=0;
  unsigned short header=0;
  do{
    header = ReadData(check, header);
    if(check)  break;

    if((header & 0xfff0) == 0xeb90)
      flag = 1;
  }while(!flag);
  raw.seekg(-2, std::ios::cur);
  
  return check;
}

int CP80256::ReadData(char *ptr){
  int flag = 0;

  for(int i=0; i<54; i++){
    char data;
    raw.read(&data, 1);
    if(raw.eof()){
      raw.close();
      raw.open(FileName(1));
      if(raw)
	std::cout << FileName() << std::endl;
      else
	return flag = 2;
      raw.read(&data, 1);
    }

    ptr[i] = data;
  }
  
  return flag;
}

unsigned CP80256::ReadData(int &flag, unsigned short initial){
  flag = 0;

  char data;
  raw.read(&data, 1);
  if(raw.eof()){
    raw.close();
    raw.open(FileName(1));
    if(raw)
      std::cout << FileName() << std::endl;
    else
      return flag = 2;
    raw.read(&data, 1);
  }

  unsigned short val = (initial & 0xffff) << 8 | (data & 0xff);
  return val;
}
  

CP80256::CP80256(std::string d_name, int b_id){
  dir = d_name;
  id = b_id;
  N_file = 0;
  pre_id = 0;
}

CP80256::~CP80256(){
}
