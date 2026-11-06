#ifndef TPC_Encoder_2p_h
#define TPC_Encoder_2p_h 1

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

class TPC_Encoder_2p {
 public:
  TPC_Encoder_2p(std::string, bool, int, bool);
  ~TPC_Encoder_2p();

  int         Init();
  int         GetEventID(unsigned&);

  unsigned    TrackSize()                  {return track.size();};
  int         TrackData(int ntr)           {return track[ntr];};
  short       FADC_data(int a, int b)      {return fadc[a][b];};
  // header word at bytes 8..11 (SMILE_tool_box RawRead2p: anode_time[])
  unsigned    EventTime() const            {return evt_time;};

 private:
  unsigned         evt_time = 0;
  std::string      FileName(bool);
  bool             OpenNext();
  unsigned         ReadData_2p(int&);
  unsigned         ReadData_2p(int&, unsigned);
  
  std::string      dir;
  bool             ac_flag, rev_flag;
  int              id, N_file;
  std::ifstream    raw;
  short            fadc[4][512];
  std::vector<int> track;
};

#endif
