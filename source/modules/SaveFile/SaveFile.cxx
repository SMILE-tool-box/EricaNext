#include "SaveFile.h"
#include <cstdio>
#include <iostream>

using namespace anlnext;

SaveFile::SaveFile() :
  m_FileName("output.root"), m_RootFile(0)
{
}

ANLStatus SaveFile::mod_define()
{
  define_parameter("filename", &mod_class::m_FileName);
  set_parameter_description("Output file name");
  return AS_OK;
}

ANLStatus SaveFile::mod_initialize()
{
  m_RootFile = new TFile(m_FileName.c_str(), "recreate");
  if ( !m_RootFile ) {
    std::cout << "SaveFile: cannot create ROOT file" << std::endl;
    return AS_QUIT;
  }
  
  return AS_OK;
}

ANLStatus SaveFile::mod_finalize()
{
  std::cout << "SaveFile: saving data to ROOT file" << std::endl;
  m_RootFile->Write();
  std::cout << "SaveFile: closing ROOT file" << std::endl;
  m_RootFile->Close();
  std::cout << "SaveFile: ROOT file closed " << std::endl;
  
  return AS_OK;
}
