#include "PSA_CenterGravity.h"

ANLStatus PSA_CenterGravity::mod_define(){
  define_parameter("save_maps",          &mod_class::save_maps_);
  define_parameter("map_peak_id_min",    &mod_class::map_peak_id_min_);
  define_parameter("map_skip_saturated", &mod_class::map_skip_saturated_);
  define_parameter("map_hg_min",         &mod_class::map_hg_min_);
  return AS_OK;
}

ANLStatus PSA_CenterGravity::mod_initialize(){
  if(!exist_module("PSA_ReadData") || !exist_module("PSA_CalcFADC"))
    return AS_QUIT;
  get_module("PSA_ReadData", &rd_);
  get_module("PSA_CalcFADC", &fadc_);
  nb_ = rd_->NBoard();
  x_hg_.assign(nb_, std::vector<double>(N_CH, 0.));
  y_hg_ = x_lg_ = y_lg_ = x_hg_;

  if(exist_module("SaveFile") && save_maps_){
    SaveFile* saveModule = nullptr;
    get_module_NC("SaveFile", &saveModule);
    saveModule->cd();
    saveModule->GetDirectory()->mkdir("PSA_CenterGravity");
    saveModule->GetDirectory()->cd("PSA_CenterGravity");
    maps_.assign(nb_, std::vector<TH2D*>(N_CH, nullptr));
    for(int i=0; i<nb_; i++)
      for(int ch=0; ch<N_CH; ch++){
        char name[32];
        snprintf(name, sizeof(name), "Map_ha%02d_ch%d", rd_->BoardID(i), ch);
        maps_[i][ch] = new TH2D(name, (std::string(name) + ";x_hg;y_hg").c_str(),
                                300, -0.32, 0.32, 300, -0.16, 0.16);
      }
  }
  return AS_OK;
}

ANLStatus PSA_CenterGravity::mod_analyze(){
  for(int i=0; i<nb_; i++){
    if(!rd_->Present(i))  continue;
    for(int ch=0; ch<N_CH; ch++){
      if(!fadc_->Hit(i, ch))  continue;
      for(int g=0; g<2; g++){
        int o = 4 * g;
        double s0 = fadc_->Sum(i, ch, o), s1 = fadc_->Sum(i, ch, o+1);
        double s2 = fadc_->Sum(i, ch, o+2), s3 = fadc_->Sum(i, ch, o+3);
        double sum = s0 + s1 + s2 + s3;
        double x = 0, y = 0;
        if(sum != 0){
          x = ( s0 - s1 - s2 + s3) / sum;
          y = (-s0 - s1 + s2 + s3) / sum;
        }
        if(g == 0){ x_hg_[i][ch] = x; y_hg_[i][ch] = y; }
        else      { x_lg_[i][ch] = x; y_lg_[i][ch] = y; }
      }
      if(!maps_.empty()){
        if(fadc_->PeakID(i, ch) < map_peak_id_min_)  continue;
        if(map_skip_saturated_ && (fadc_->SatuH(i, ch) || fadc_->SatuL(i, ch)))  continue;
        if(fadc_->HG(i, ch) < map_hg_min_)  continue;
        maps_[i][ch]->Fill(x_hg_[i][ch], y_hg_[i][ch]);
      }
    }
  }
  return AS_OK;
}

PSA_CenterGravity::PSA_CenterGravity(){
  save_maps_ = true;
  map_peak_id_min_ = 0;
  map_skip_saturated_ = true;
  map_hg_min_ = 0.;
}

PSA_CenterGravity::~PSA_CenterGravity(){}
