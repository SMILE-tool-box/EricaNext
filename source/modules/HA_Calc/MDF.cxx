#include "MDF.h"

void MDF::SetParameters(const char *fname, int Npmt){
  N_PMT = Npmt;
  ifstream para_file(fname);
  if(!para_file){
    cerr << " ERROR : Can't find " << fname << endl;
    exit(1);
  }

  gNVariables = new int [N_PMT];
  gNCoefficients = new int [N_PMT];
  gDMean = new double [N_PMT];
  tMean = new double* [N_PMT];
  tMax = new double* [N_PMT];
  tMin = new double* [N_PMT];
  tCof = new double* [N_PMT];
  tRMS = new double* [N_PMT];
  tPow = new int* [N_PMT];
  val = new double* [N_PMT];
  for(int pmt_index=0; pmt_index<N_PMT; pmt_index++){
    para_file >> gNVariables[pmt_index] 
	      >> gNCoefficients[pmt_index]
	      >> gDMean[pmt_index];
    val[pmt_index] = new double [(gNVariables[pmt_index])];

    tMean[pmt_index] = new double [(gNVariables[pmt_index])];
    for(int i=0; i<gNVariables[pmt_index]; i++)
      para_file >> tMean[pmt_index][i];

    tMin[pmt_index] = new double [(gNVariables[pmt_index])]; 
    for(int i=0; i<gNVariables[pmt_index]; i++)
      para_file >> tMin[pmt_index][i];

    tMax[pmt_index] = new double [(gNVariables[pmt_index])];
    for(int i=0; i<gNVariables[pmt_index]; i++)
      para_file >> tMax[pmt_index][i];
    
    tCof[pmt_index] = new double [(gNCoefficients[pmt_index])];
    for(int i=0; i<gNCoefficients[pmt_index]; i++)
      para_file >> tCof[pmt_index][i];

    tRMS[pmt_index] = new double [(gNCoefficients[pmt_index])];
    for(int i=0; i<gNCoefficients[pmt_index]; i++)
      para_file >> tRMS[pmt_index][i];

    tPow[pmt_index] 
      = new int [(gNVariables[pmt_index]*gNCoefficients[pmt_index])];
    for(int i=0; i<gNCoefficients[pmt_index]; i++)
      for(int k=0; k<gNVariables[pmt_index]; k++)
	para_file >> tPow[pmt_index][gNVariables[pmt_index]*i+k];
  }
}

double MDF::ConvertMDF(int pmt_id, double *rx){
  for(int i=0; i<gNVariables[pmt_id]; i++)
    val[pmt_id][i] = rx[i];

  double rV = gDMean[pmt_id];
  for(int index=0; index<gNCoefficients[pmt_id]; index++){
    double term = tCof[pmt_id][index];
    for(int i=0; i<gNVariables[pmt_id]; i++){
      int power = tPow[pmt_id][index*gNVariables[pmt_id]+i];
      //double p1=1;
      double p2=0, p3=0, r=0;
      double v 
	= 1 + 2./(tMax[pmt_id][i]-tMin[pmt_id][i])
	* (val[pmt_id][i] - tMax[pmt_id][i]);

      switch(power){
      case 1:
	r = 1;
	break;
      case 2:
	r = v;
	break;
      default:
	p2 = v;
	for(int j=3; j<=power; j++){
	  p3 = p2*v;
	  //p1 = p2;
	  p2 = p3;
	}
	r = p3;
      }

      term *= r;
    }
    rV += term;
  }

  return rV;
}

MDF::MDF(){
}

MDF::MDF(const char *dirname, int Npmt){
  SetParameters(dirname, Npmt);
}

MDF::~MDF(){
  for(int i=0; i<N_PMT; i++){
    delete [] tMean[i];
    delete [] tMin[i];
    delete [] tMax[i];
    delete [] tCof[i];
    delete [] tRMS[i];
    delete [] tPow[i];
    delete [] val[i];
  }
  delete [] tMean;
  delete [] tMin;
  delete [] tMax;
  delete [] tCof;
  delete [] tRMS;
  delete [] tPow;
  delete [] val;

  delete [] gNVariables;
  delete [] gNCoefficients;
  delete [] gDMean;
}
