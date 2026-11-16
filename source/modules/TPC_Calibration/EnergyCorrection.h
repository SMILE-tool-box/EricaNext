#ifndef ERICA_TPC_ENERGY_CORRECTION_H
#define ERICA_TPC_ENERGY_CORRECTION_H
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>

// Quadratic between calibration anchors, endpoint tangents outside; keV in/out.
class TPCEnergyCorrection {
 public:
  bool enabled=false;
  double a0=0,a1=1,a2=0,lo=0,hi=0;
  void Load(const std::string& path){
    std::ifstream in(path);std::string magic,key,outside,extra;
    if(!(in>>magic) || magic!="TPC_ENERGY_QUADRATIC_V1" ||
       !(in>>key>>a0>>a1>>a2) || key!="coefficients" ||
       !(in>>key>>lo>>hi) || key!="range" ||
       !(in>>key>>outside) || key!="outside" || outside!="tangent" || (in>>extra) ||
       !std::isfinite(a0) || !std::isfinite(a1) || !std::isfinite(a2) ||
       !std::isfinite(lo) || !std::isfinite(hi) || hi<=lo ||
       std::min(a1+2*a2*lo,a1+2*a2*hi)<=0)
      throw std::runtime_error("invalid TPC energy correction: "+path);
    enabled=true;
  }
  double Apply(double energy) const {
    if(!enabled)return energy;
    const double x=std::max(lo,std::min(hi,energy));
    return std::max(0.,a0+a1*x+a2*x*x+(a1+2*a2*x)*(energy-x));
  }
};
#endif
