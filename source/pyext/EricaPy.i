%module EricaPy
%{
#include <anlnext/BasicModule.hh>

// include headers of my modules
#include "SaveFile.h"
#include "ReadData.h"
#include "CalcFADC.h"
#include "TPC_Calibration.h"
#include "TOT_Skewness.h"
#include "Reconstruct.h"
#include "RawTreeWriter_sm3.h"
#include "PSA_ReadData.h"
#include "PSA_CalcFADC.h"
#include "PSA_CenterGravity.h"
#include "PSA_Calibration.h"
#include "PSA_TreeWriter.h"
#include "PSA_Event.h"
// SMILE-2+ (from EricaNext_try)
#include "ReadData_2p.h"
#include "CalcFADC_2p.h"
#include "TPC_Calibration_2p.h"
#include "TOT_Skewness_2p.h"
#include "HA_CenterGravity_2p.h"
#include "HA_Calc_2p.h"
#include "Reconstruct_2p.h"
#include "RawTreeWriter_2p.h"

%}

%import(module="anlnext.anlnextpy") "anlnext/python/anlnextpy.i"


// interface to my modules

class SaveFile         : public anlnext::BasicModule {};
class ReadData         : public anlnext::BasicModule {};
class CalcFADC         : public anlnext::BasicModule {};
class TPC_Calibration  : public anlnext::BasicModule {};
class TOT_Skewness     : public anlnext::BasicModule {};
class Reconstruct      : public anlnext::BasicModule {};
class RawTreeWriter_sm3: public anlnext::BasicModule {};
class PSA_ReadData     : public anlnext::BasicModule {};
class PSA_CalcFADC     : public anlnext::BasicModule {};
class PSA_CenterGravity: public anlnext::BasicModule {};
class PSA_Calibration  : public anlnext::BasicModule {};
class PSA_TreeWriter   : public anlnext::BasicModule {};
class PSA_Event        : public anlnext::BasicModule {};
// SMILE-2+ (from EricaNext_try)
class ReadData_2p      : public anlnext::BasicModule {};
class CalcFADC_2p      : public anlnext::BasicModule {};
class TPC_Calibration_2p: public anlnext::BasicModule {};
class TOT_Skewness_2p  : public anlnext::BasicModule {};
class HA_CenterGravity_2p: public anlnext::BasicModule {};
class HA_Calc_2p       : public anlnext::BasicModule {};
class Reconstruct_2p   : public anlnext::BasicModule {};
class RawTreeWriter_2p : public anlnext::BasicModule {};
