#ifndef CP80256_h
#define CP80256_h 1

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

class CP80256 {
 public:
  CP80256(std::string, int);
  ~CP80256();

  int            Init();
  int            GetEventID(unsigned&);
  
  std::string    FileName(bool);
  unsigned       ReadData(int&, unsigned short);
  int            ReadData(char*);
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
