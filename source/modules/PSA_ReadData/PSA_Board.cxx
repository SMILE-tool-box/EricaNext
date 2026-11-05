#include "PSA_Board.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
const unsigned char HEADER[4] = {0xeb, 0x90, 0x19, 0x64};
const unsigned char FOOTER[4] = {0x75, 0x50, 0x49, 0x43};
constexpr size_t FRAME_MIN   = 40;    // header .. temperatures .. footer, no ADC data
constexpr size_t DATA_START  = 0x24;
constexpr size_t BLOCK       = 16;
constexpr size_t ADC_BYTES   = PSA_Board::N_SAMPLE * BLOCK;   // 800 per ADC with data

inline unsigned be32(const unsigned char* p){
  return (unsigned(p[0]) << 24) | (unsigned(p[1]) << 16) | (unsigned(p[2]) << 8) | unsigned(p[3]);
}
inline bool match(const unsigned char* p, const unsigned char* q){
  return std::memcmp(p, q, 4) == 0;
}
// data count bits of the frame (bit 5-j for ADC j)
inline unsigned datacnt(const unsigned char* ev){
  return ((((unsigned)ev[13] << 8) | ev[14]) >> 6) & 0x3f;
}
}

PSA_Board::PSA_Board(int id, const std::vector<std::string>& files)
  : id_(id), files_(files) {
  std::memset(temp_raw_, 0, sizeof(temp_raw_));
  std::memset(adc_, 0, sizeof(adc_));
  std::memset(satu_h_, 0, sizeof(satu_h_));
  std::memset(satu_l_, 0, sizeof(satu_l_));
}

std::string PSA_Board::Name(int id){
  char name[16];
  snprintf(name, sizeof(name), "ha%02d", id);
  return name;
}

int PSA_Board::NHit() const {
  int n = 0;
  for(int ch=0; ch<N_CH; ch++)  n += Hit(ch);
  return n;
}

// Append the next file to what is left of the buffer (a frame may continue into the next file).
bool PSA_Board::LoadNextFile(){
  if(ifile_ >= files_.size())  return false;
  std::ifstream in(files_[ifile_], std::ios::binary);
  if(!in){
    std::cerr << "PSA_Board: cannot open " << files_[ifile_] << std::endl;
    ifile_++;
    return LoadNextFile();
  }
  std::cout << files_[ifile_] << std::endl;
  ifile_++;
  std::vector<unsigned char> rest(buf_.begin() + pos_, buf_.end());
  rest.insert(rest.end(), std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  buf_.swap(rest);
  pos_ = 0;
  return true;
}

// Find the frame at pos_ (after skipping to a header). len = frame length.
// The length is 40 + 800 x (ADCs with data); when the footer is not there
// (broken frame), the first footer after the header is used, as psa_decode.py does.
bool PSA_Board::FindFrame(size_t& len){
  for(;;){
    // header
    size_t i = pos_;
    while(i + 4 <= buf_.size() && !match(&buf_[i], HEADER))  i++;
    if(i + 4 > buf_.size()){
      size_t keep = buf_.size() - pos_ < 3 ? buf_.size() - pos_ : 3;   // a header may straddle files
      n_skipped_ += buf_.size() - pos_ - keep;
      pos_ = buf_.size() - keep;
      if(!LoadNextFile())  return false;
      continue;
    }
    n_skipped_ += i - pos_;
    pos_ = i;

    if(buf_.size() - pos_ < FRAME_MIN){
      if(!LoadNextFile())  return false;
      continue;
    }
    unsigned dc = datacnt(&buf_[pos_]);
    int nadc = 0;
    for(int j=0; j<N_CH; j++)  nadc += (dc >> j) & 1;
    size_t expect = FRAME_MIN + ADC_BYTES * nadc;
    if(buf_.size() - pos_ < expect){
      if(LoadNextFile())  continue;
      // last file: fall through to the footer search on what is left
    }
    else if(match(&buf_[pos_ + expect - 4], FOOTER)){
      len = expect;
      return true;
    }
    // footer search
    size_t j = pos_ + 4;
    while(j + 4 <= buf_.size() && !match(&buf_[j], FOOTER))  j++;
    if(j + 4 > buf_.size()){
      if(LoadNextFile())  continue;
      n_skipped_ += buf_.size() - pos_;
      pos_ = buf_.size();
      return false;
    }
    len = j + 4 - pos_;
    if(len < FRAME_MIN){   // not a frame: skip this header
      n_skipped_ += 4;
      pos_ += 4;
      continue;
    }
    return true;
  }
}

bool PSA_Board::Next(){
  if(end_)  return false;
  if(buf_.empty() && !LoadNextFile()){
    end_ = true;
    return false;
  }
  size_t len = 0;
  if(!FindFrame(len)){
    end_ = true;
    return false;
  }
  Decode(&buf_[pos_], len);
  pos_ += len;
  n_frames_++;
  // drop what has been read, to keep the buffer small
  if(pos_ > (size_t)64 * 1024 * 1024){
    buf_.erase(buf_.begin(), buf_.begin() + pos_);
    pos_ = 0;
  }
  return true;
}

void PSA_Board::Decode(const unsigned char* ev, size_t len){
  trig_ = be32(ev + 4);
  clk_  = be32(ev + 8);
  if(first_){
    first_ = false;
    clk0_ = clk_;
  }
  else if(trig_ < prev_trig_){
    epoch_++;
    clk0_ = clk_;
    clk_wraps_ = 0;
    std::cerr << "PSA_Board: " << Name(id_) << ": trig went back from " << prev_trig_ << " to " << trig_
              << " (frame " << n_frames_ << "), counted as a new epoch" << std::endl;
  }
  else if(clk_ < prev_clk_){
    clk_wraps_++;
  }
  prev_trig_ = trig_;
  prev_clk_  = clk_;
  time_ = ((double)clk_wraps_ * 4294967296.0 + (double)clk_ - (double)clk0_) / 1e6;

  unsigned dc = datacnt(ev);
  hit_mask_ = 0;
  for(int j=0; j<N_CH; j++)
    if((dc >> (5 - j)) & 1)  hit_mask_ |= 1u << (5 - j);   // ADC j -> ch 5-j

  unsigned w[3] = {be32(ev + 0x14), be32(ev + 0x18), be32(ev + 0x1C)};
  for(int k=0; k<3; k++){
    temp_raw_[2*k]   = (w[k] >> 14) & 0x3fff;
    temp_raw_[2*k+1] =  w[k]        & 0x3fff;
  }

  std::memset(adc_, 0, sizeof(adc_));
  std::memset(satu_h_, 0, sizeof(satu_h_));
  std::memset(satu_l_, 0, sizeof(satu_l_));
  if(len < DATA_START + 4)  return;
  size_t nblocks = (len - 4 - DATA_START) / BLOCK;
  for(size_t b=0; b<nblocks; b++){
    const unsigned char* p = ev + DATA_START + b * BLOCK;
    unsigned r1 = be32(p), r2 = be32(p + 4), r3 = be32(p + 8), r4 = be32(p + 12);
    unsigned adc_bit = (r1 >> 24) & 0x3f;
    int j = -1;
    for(int k=0; k<N_CH; k++)  if(adc_bit == (1u << k))  j = k;
    if(j < 0)  continue;
    int sample = (int)((r1 >> 16) & 0x7f) - 1;
    if(sample < 0 || sample >= N_SAMPLE)  continue;
    unsigned short v[N_SIG];
    v[0] = (r1 >> 2) & 0x3fff;
    v[1] = ((r1 << 12) & 0x3000) | ((r2 >> 20) & 0x0fff);
    v[2] = (r2 >> 6) & 0x3fff;
    v[3] = ((r2 << 8) & 0x3f00) | ((r3 >> 24) & 0x00ff);
    v[4] = (r3 >> 10) & 0x3fff;
    v[5] = ((r3 << 4) & 0x3ff0) | ((r4 >> 26) & 0x000f);
    v[6] = (r4 >> 14) & 0x3fff;
    v[7] =  r4        & 0x3fff;
    int ch = 5 - j;
    for(int s=0; s<N_SIG; s++){
      adc_[ch][sample][s] = v[s];
      if(v[s] >= ADC_MAX){
        if(s < 4)  satu_h_[ch] = true;
        else       satu_l_[ch] = true;
      }
    }
  }
}
