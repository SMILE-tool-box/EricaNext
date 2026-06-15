#ifndef MDF_h
#define MDF_h

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <TMath.h>
#include <unistd.h>

using namespace std;


class MDF {
 public:
  MDF();
  MDF(const char*, int); 
  ~MDF();

  void    SetParameters(const char*, int);
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
