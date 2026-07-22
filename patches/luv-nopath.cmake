file(READ "${SOURCE_DIR}/cmake/Modules/FindLibuv.cmake" _content)
string(REPLACE
  "FIND_LIBRARY(LIBUV_LIBRARIES NAMES uv libuv)"
  "FIND_LIBRARY(LIBUV_LIBRARIES NAMES uv libuv NO_DEFAULT_PATH)"
  _content "${_content}")
file(WRITE "${SOURCE_DIR}/cmake/Modules/FindLibuv.cmake" "${_content}")
