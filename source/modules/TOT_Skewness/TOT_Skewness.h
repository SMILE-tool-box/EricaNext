#ifndef TOT_skewness_h
#define TOT_skewness_h 1

#include <anlnext/BasicModule.hh>
#include <sstream>

#include "SaveFile.h"
#include "ReadData.h"

using namespace anlnext;

class TOT_Skewness : public BasicModule {
  DEFINE_ANL_MODULE(TOT_Skewness, 1.0);

 public:
  TOT_Skewness();
  ~TOT_Skewness();

  ANLStatus mod_define() override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze() override;
  ANLStatus mod_finalize() override;
  
 private:
};

#endif
