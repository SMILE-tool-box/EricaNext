#ifndef PSA_ReadData_h
#define PSA_ReadData_h 1

#include <anlnext/BasicModule.hh>
#include <string>
#include <vector>

#include <TH2.h>

#include "SaveFile.h"
#include "PSA_Board.h"

using namespace anlnext;

class ReadData;

// Reads the SMILE-3 scintillator (PSA) raw data, ha<NN>_<number>.dat of all HA
// boards found in the run directory (and extra_data_dirs, e.g. the other storage
// node), and gives one event per trigger:
//   - without ReadData in the chain (HA_cal etc.), the boards are merged by the
//     trigger counter: an event is the smallest (epoch, trig) of the boards, and
//     the boards with that trig are "present". This works when every board sends
//     every trigger (HA_cal) and when only the boards with a hit send a frame.
//   - with ReadData before this module and sync_tpc = true, the event of ReadData
//     (its trigger counter, ReadData.event_id = true) is followed: boards whose
//     trig equals it are present, frames of later triggers are kept.
// The PSA_* modules read the data through Board(i) for present boards.
class PSA_ReadData : public BasicModule {
  DEFINE_ANL_MODULE(PSA_ReadData, 1.0);

 public:
  PSA_ReadData();
  ~PSA_ReadData();

  ANLStatus mod_define()     override;
  ANLStatus mod_initialize() override;
  ANLStatus mod_analyze()    override;
  ANLStatus mod_finalize()   override;

  int              NBoard()        const { return (int)boards_.size(); }
  int              BoardID(int i)  const { return boards_[i]->ID(); }
  const std::vector<int>* Get_BoardID() const { return &board_ids_; }
  // index in Board() of HA board <id>, -1 when it is not in this run
  int              Index(int id)   const;
  bool             Present(int i)  const { return present_[i]; }
  const PSA_Board& Board(int i)    const { return *boards_[i]; }
  unsigned         Trig()          const { return trig_; }
  int              Epoch()         const { return epoch_; }
  unsigned         SEvtID()        const { return sevt_id_; }   // 0-based count of the events given
  int              NPresent()      const { return n_present_; }
  bool             SyncTPC()       const { return sync_; }

 private:
  void ScanDir(const std::string& dir);

  std::string data_dir_;
  std::vector<std::string> extra_data_dirs_;
  bool sync_tpc_;
  bool save_hit_pattern_;

  std::vector<PSA_Board*> boards_;
  std::vector<int> board_ids_;
  std::vector<bool> present_, consumed_;
  std::vector<std::vector<std::string>> files_;   // per board id 0-99
  const ReadData* tpc_ = nullptr;
  bool sync_ = false;
  unsigned trig_ = 0;
  int epoch_ = 0, n_present_ = 0;
  unsigned sevt_id_ = 0, next_sevt_id_ = 0;
  long n_tmp_files_ = 0;
  TH2D* hit_pattern_ = nullptr;
};

#endif
