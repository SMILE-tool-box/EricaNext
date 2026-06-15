########################################################
set(TARGET_EXT_LIBRARY EricaPy)
set(SWIG_IF_FILE ${TARGET_EXT_LIBRARY}.i)
set(CLASS_LIST_FILE class_list_${TARGET_EXT_LIBRARY}.hh)
set(PYTHON_EXT_INCLUDE_DIRS
  ${MOD_DIRS}
  ${ANLNEXT_INC_DIR}
  )
set(PYTHON_EXT_LIBRARY_DIRS
  ${ANLNEXT_LIB_DIR}
  )
set(PYTHON_EXT_LIBRARIES
  ${ANLNEXT_LIB}
  Erica
  )

########################################################
set(cxx_definitions )
set(SWIG_FLAGS ${cxx_definitions})
add_definitions(${cxx_definitions})

########################################################
include(CreateSwigPython)
