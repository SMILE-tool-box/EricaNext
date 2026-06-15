%module EricaPy
%{
#include <anlnext/BasicModule.hh>

// include headers of my modules
#include "SaveFile.h"
#include "ReadData.h"
#include "HA_CenterGravity.h"
#include "HA_Calc.h"
#include "CalcFADC.h"
#include "TPC_Calibration.h"
#include "TOT_Skewness.h"
#include "Reconstruct.h"

%}

%import(module="anlnext.anlnextpy") "anlnext/python/anlnextpy.i"


// interface to my modules

class SaveFile         : public anlnext::BasicModule {};
class ReadData         : public anlnext::BasicModule {};
class HA_CenterGravity : public anlnext::BasicModule {};
class HA_Calc          : public anlnext::BasicModule {};
class CalcFADC         : public anlnext::BasicModule {};
class TPC_Calibration  : public anlnext::BasicModule {};
class TOT_Skewness     : public anlnext::BasicModule {};
class Reconstruct      : public anlnext::BasicModule {};
