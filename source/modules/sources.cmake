set(SUB_DIRS
  SaveFile
  ReadData
  CalcFADC
  TPC_Calibration
  TOT_Skewness
  Reconstruct
  RawTreeWriter
  PSA_ReadData
  PSA_CalcFADC
  PSA_CenterGravity
  PSA_Calibration
  PSA_TreeWriter
  PSA_Event
  smile2p/ReadData_2p
  smile2p/CalcFADC_2p
  smile2p/TPC_Calibration_2p
  smile2p/TOT_Skewness_2p
  smile2p/HA_CenterGravity_2p
  smile2p/HA_Calc_2p
  smile2p/Reconstruct_2p
  smile2p/RawTreeWriter_2p
)

foreach(dir ${SUB_DIRS})
  set(CUR_DIR modules/${dir})
  include(${CUR_DIR}/sources.cmake)
  set(MOD_DIRS ${CUR_DIR}/ ${MOD_DIRS} )
endforeach()
