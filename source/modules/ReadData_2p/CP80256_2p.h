#ifndef CP80256_2p_h
#define CP80256_2p_h 1

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <filesystem>
#include <vector>
#include <anlnext/BasicModule.hh>

using namespace anlnext;

class CP80256_2p {
 public:
  CP80256_2p(std::string, int);
  ~CP80256_2p();

  int            Init();
  int            GetEventID(unsigned&);
  
  std::string    FileName(bool);
  unsigned       ReadData_2p(int&, unsigned short);
  int            ReadData_2p(char*);
  bool           GetADC(int ch, short &data){
    bool flag = (adc[ch] >> 13) & 0x1;
    data = adc[ch] & 0x0fff;
    return flag;
  };
  
 private:
  std::string    dir;
  int            id, N_file;
  std::ifstream  raw;
  unsigned       pre_id;
  short          adc[24];
};

#endif
