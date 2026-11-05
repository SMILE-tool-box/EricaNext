set(SUB_DIRS
  SaveFile
  ReadData
  HA_CenterGravity
  HA_Calc
  CalcFADC
  TPC_Calibration
  TOT_Skewness
  Reconstruct
  RawTreeWriter_sm3
  PSA_ReadData
  PSA_CalcFADC
  PSA_CenterGravity
  PSA_Calibration
  PSA_TreeWriter
)

foreach(dir ${SUB_DIRS})
  set(CUR_DIR modules/${dir})
  include(${CUR_DIR}/sources.cmake)
  set(MOD_DIRS ${CUR_DIR}/ ${MOD_DIRS} )
endforeach()
