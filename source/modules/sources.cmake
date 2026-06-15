set(SUB_DIRS
  SaveFile
  ReadData
  HA_CenterGravity
  HA_Calc
  CalcFADC
  TPC_Calibration
  TOT_Skewness
  Reconstruct
)

foreach(dir ${SUB_DIRS})
  set(CUR_DIR modules/${dir})
  include(${CUR_DIR}/sources.cmake)
  set(MOD_DIRS ${CUR_DIR}/ ${MOD_DIRS} )
endforeach()
