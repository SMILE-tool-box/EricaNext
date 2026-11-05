#include "PSA_TreeWriter.h"

ANLStatus PSA_TreeWriter::mod_define(){
  define_parameter("save_waveform", &mod_class::save_waveform_);
  return AS_OK;
}

ANLStatus PSA_TreeWriter::mod_initialize(){
  if(!exist_module("SaveFile") || !exist_module("PSA_ReadData") || !exist_module("PSA_CalcFADC"))
    return AS_QUIT;
  get_module("PSA_ReadData", &rd_);
  get_module("PSA_CalcFADC", &fadc_);
  if(exist_module("PSA_CenterGravity"))  get_module("PSA_CenterGravity", &cg_);
  if(exist_module("PSA_Calibration"))    get_module("PSA_Calibration", &cal_);

  SaveFile* saveModule = nullptr;
  get_module_NC("SaveFile", &saveModule);
  saveModule->cd();
  saveModule->GetDirectory()->mkdir("PSA_TreeWriter");
  saveModule->GetDirectory()->cd("PSA_TreeWriter");

  tree_ = new TTree("psa", "PSA hits, one row per hit channel");
  tree_->Branch("sevt_id",   &sevt_id_,   "sevt_id/i");
  tree_->Branch("trig",      &trig_,      "trig/i");
  tree_->Branch("epoch",     &epoch_,     "epoch/I");
  tree_->Branch("clk",       &clk_,       "clk/i");
  tree_->Branch("time",      &time_,      "time/D");
  tree_->Branch("board",     &board_,     "board/S");
  tree_->Branch("ch",        &ch_,        "ch/S");
  tree_->Branch("hitnum",    &hitnum_,    "hitnum/S");
  tree_->Branch("hg",        &hg_,        "hg/D");
  tree_->Branch("lg",        &lg_,        "lg/D");
  tree_->Branch("wave_sum",  wave_sum_,   "wave_sum[8]/F");
  tree_->Branch("ped",       ped_,        "ped[8]/F");
  tree_->Branch("peak_id",   &peak_id_,   "peak_id/S");
  tree_->Branch("satu_h",    &satu_h_,    "satu_h/O");
  tree_->Branch("satu_l",    &satu_l_,    "satu_l/O");
  tree_->Branch("temp",      temp_,       "temp[6]/F");
  tree_->Branch("temp_mean", &temp_mean_, "temp_mean/F");
  if(cg_){
    tree_->Branch("x_hg", &x_hg_, "x_hg/D");
    tree_->Branch("y_hg", &y_hg_, "y_hg/D");
    tree_->Branch("x_lg", &x_lg_, "x_lg/D");
    tree_->Branch("y_lg", &y_lg_, "y_lg/D");
  }
  if(cal_){
    tree_->Branch("x_corr", &x_corr_, "x_corr/D");
    tree_->Branch("y_corr", &y_corr_, "y_corr/D");
    tree_->Branch("pix_n",  &pix_n_,  "pix_n/S");
    tree_->Branch("pix_m",  &pix_m_,  "pix_m/S");
    tree_->Branch("pixel",  &pixel_,  "pixel/S");
    tree_->Branch("energy", &energy_, "energy/D");
  }
  if(save_waveform_)
    tree_->Branch("adc", adc_, "adc[400]/s");   // [sample][HG0-3, LG0-3], raw ADC
  return AS_OK;
}

ANLStatus PSA_TreeWriter::mod_analyze(){
  sevt_id_ = rd_->SEvtID();
  trig_    = rd_->Trig();
  epoch_   = rd_->Epoch();
  for(int i=0; i<rd_->NBoard(); i++){
    if(!rd_->Present(i))  continue;
    const PSA_Board& b = rd_->Board(i);
    clk_    = b.Clk();
    time_   = b.Time();
    board_  = b.ID();
    hitnum_ = b.NHit();
    for(int k=0; k<6; k++)  temp_[k] = fadc_->Temp(i, k);
    temp_mean_ = fadc_->TempMean(i);
    for(int ch=0; ch<PSA_Board::N_CH; ch++){
      if(!fadc_->Hit(i, ch))  continue;
      ch_ = ch;
      hg_ = fadc_->HG(i, ch);
      lg_ = fadc_->LG(i, ch);
      for(int s=0; s<8; s++){
        wave_sum_[s] = fadc_->Sum(i, ch, s);
        ped_[s]      = fadc_->Ped(i, ch, s);
      }
      peak_id_ = fadc_->PeakID(i, ch);
      satu_h_  = fadc_->SatuH(i, ch);
      satu_l_  = fadc_->SatuL(i, ch);
      if(cg_){
        x_hg_ = cg_->XHG(i, ch);  y_hg_ = cg_->YHG(i, ch);
        x_lg_ = cg_->XLG(i, ch);  y_lg_ = cg_->YLG(i, ch);
      }
      if(cal_){
        x_corr_ = cal_->XCorr(i, ch);
        y_corr_ = cal_->YCorr(i, ch);
        pix_n_  = cal_->PixN(i, ch);
        pix_m_  = cal_->PixM(i, ch);
        pixel_  = cal_->Pixel(i, ch);
        energy_ = cal_->Energy(i, ch);
      }
      if(save_waveform_)
        for(int r=0; r<PSA_Board::N_SAMPLE; r++)
          for(int s=0; s<PSA_Board::N_SIG; s++)
            adc_[r * PSA_Board::N_SIG + s] = b.ADC(ch, r, s);
      tree_->Fill();
    }
  }
  return AS_OK;
}

PSA_TreeWriter::PSA_TreeWriter(){
  save_waveform_ = false;
}

PSA_TreeWriter::~PSA_TreeWriter(){}
