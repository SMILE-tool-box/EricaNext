#ifndef MDF_2p_h
#define MDF_2p_h

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <TMath.h>
#include <unistd.h>

using namespace std;


class MDF_2p {
 public:
  MDF_2p();
  MDF_2p(const char*, int); 
  ~MDF_2p();

  void    SetParameters(const char*, int);
  // SMILE_tool_box HA_Calc_s2::Init(): XMDF_HA%02d.dat/YMDF_HA%02d.dat are a
  // flat 110-double-per-PMT TMultiDimFit dump (npar, nterm, mean, tMean[2],
  // tMin[2], tMax[2], tCof[0..nterm-1] from index 10, tPow[i*npar+j] from
  // index 60), unlike the older header+variable-length format SetParameters()
  // reads. Populates the same internal arrays ConvertMDF() evaluates.
  void    SetParametersFlat(const char*, int);
  double  ConvertMDF(int, double*);

 private:
  int       N_PMT;
  int      *gNVariables, *gNCoefficients;
  double   *gDMean;
  double  **tMean, **tMax, **tMin;
  double  **tCof, **tRMS, **val;
  int     **tPow;
};

#endif
