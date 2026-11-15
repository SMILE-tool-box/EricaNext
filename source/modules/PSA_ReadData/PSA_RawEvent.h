#ifndef ERICA_PSA_RAW_EVENT_H
#define ERICA_PSA_RAW_EVENT_H
#include <TTree.h>
#include <vector>
#include <stdexcept>
struct PSA_RawEvent {
  unsigned trig=0,sevt=0; int epoch=0;
  std::vector<int> board, board_epoch;
  std::vector<unsigned> clk,mask;
  std::vector<double> time;
  std::vector<unsigned short> temp,adc;
  void Branch(TTree* t){
    t->Branch("event_id",&trig);t->Branch("sevt_id",&sevt);t->Branch("epoch",&epoch);
    t->Branch("board",&board);t->Branch("board_epoch",&board_epoch);t->Branch("clk",&clk);
    t->Branch("hit_mask",&mask);t->Branch("time",&time);t->Branch("temp_raw",&temp);t->Branch("adc",&adc);
  }
};
#endif
