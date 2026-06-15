#ifndef TPC_Encoder_h
#define TPC_Encoder_h 1

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <vector>
#include <anlnext/BasicModule.hh>

using namespace anlnext;

class TPC_Encoder {
 public:
  TPC_Encoder(std::string, bool, int, bool, bool);
  ~TPC_Encoder();

  int         Init();
  int         GetEventID(unsigned&);

  unsigned    TrackSize()                  {return track.size();};
  int         TrackData(int ntr)           {return track[ntr];};
  short       FADC_data(int a, int b)      {return fadc[a][b];};
  unsigned    GetNtime()                   {return m_N_time;};
  
  
 private:
  std::string      FileName(bool);
  unsigned         ReadData(int&);
  unsigned         ReadData(int&, unsigned);
  
  std::string      dir;
  bool             ac_flag, rev_flag, ntime_flag;
  int              id, N_file;
  std::ifstream    raw;
  short            fadc[4][512];
  unsigned         m_N_time = 0;
  std::vector<int> track;
};

#endif
