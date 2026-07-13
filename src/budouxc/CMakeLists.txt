cmake_minimum_required(VERSION 3.25)
project(budouxc LANGUAGES C)

set(BUDOUXC_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/budouxc")

set(BUDOUXC_SOURCES
  ${BUDOUXC_SRC_DIR}/src/budoux.c
)

set(BUDOUXC_HEADERS
  ${BUDOUXC_SRC_DIR}/include/budoux.h
)

add_library(budouxc STATIC ${BUDOUXC_SOURCES} ${BUDOUXC_HEADERS})
target_include_directories(budouxc PUBLIC
  $<BUILD_INTERFACE:${BUDOUXC_SRC_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(budouxc PROPERTIES
  C_STANDARD 17
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
  POSITION_INDEPENDENT_CODE ON
)

install(TARGETS budouxc
  EXPORT budouxcConfig
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${BUDOUXC_HEADERS} DESTINATION include)
install(EXPORT budouxcConfig
  NAMESPACE budouxc::
  DESTINATION lib/cmake/budouxc
)
