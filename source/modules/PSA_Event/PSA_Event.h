#ifndef PSA_Event_h
#define PSA_Event_h 1

#include <anlnext/BasicModule.hh>
#include <string>
#include <vector>

#include <TH1.h>

#include "SaveFile.h"
#include "PSA_ReadData.h"
#include "PSA_CalcFADC.h"
#include "PSA_Calibration.h"

using namespace anlnext;

// The PSA hits of an event in detector coordinates, for Reconstruct (same interface as HA_Calc).
//
// A hit is used when PSA_Calibration gave it a pixel and an energy > energy_min, and the
// block (board, ch) is in geometry_file. Its absorption point is
//   P = (x0, y0, z0) + pitch [ (n - 4.5) u + (m - 4.5) v ]   [mm]
// with the row board,ch,x0,y0,z0,ux,uy,uz,vx,vy,vz,pitch of geometry_file
// ((x0, y0, z0): center of the 8x8 block, u: direction of increasing n, v: of increasing m,
// coordinates of tutorials/0_Introduction: origin at the center of the mu-PIC, Z downward).
//
// Event selections: PSA_SingleHit (exactly one used hit), PSA_MultiHit (more than one),
// PSA_NoHits (none). Events are not skipped here; Reconstruct uses PSA_SingleHit.
class PSA_Event : public BasicModule {
  DEFINE_ANL_MODULE(PSA_Event, 1.0);

 public:
  PSA_Event();
  ~PSA_Event();

  ANLStatus mod_define()     override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze()    override;
  ANLStatus mod_finalize()   override;

  // used hits of this event [keV], [mm]
  const std::vector<double>* DetEng() const { return &det_eng_; }
  const std::vector<double>* Det_X()  const { return &det_x_; }
  const std::vector<double>* Det_Y()  const { return &det_y_; }
  const std::vector<double>* Det_Z()  const { return &det_z_; }
  const std::vector<int>*    Det_Board() const { return &det_board_; }
  const std::vector<int>*    Det_Pixel() const { return &det_pixel_; }   // 64 ch + 8 (n-1) + (m-1)

 private:
  struct Block {
    bool ok = false;
    double p0[3], u[3], v[3], pitch;
  };
  bool LoadGeometry();

  std::string geometry_file_;
  double energy_min_;
  bool save_hists_;

  const PSA_ReadData*    rd_   = nullptr;
  const PSA_CalcFADC*    fadc_ = nullptr;
  const PSA_Calibration* cal_  = nullptr;
  int nb_ = 0;
  std::vector<std::vector<Block>> geo_;   // [board index][ch]
  std::vector<double> det_eng_, det_x_, det_y_, det_z_;
  std::vector<int> det_board_, det_pixel_;
  long n_no_geometry_ = 0;
  TH1D* n_hits_ = nullptr;
  TH1D* single_spec_ = nullptr;
};

#endif
