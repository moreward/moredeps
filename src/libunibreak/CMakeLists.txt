cmake_minimum_required(VERSION 3.25)
project(libunibreak LANGUAGES C)

set(UNIBREAK_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/libunibreak")

set(UNIBREAK_SOURCES
  ${UNIBREAK_SRC_DIR}/src/eastasianwidthdata.c
  ${UNIBREAK_SRC_DIR}/src/eastasianwidthdef.c
  ${UNIBREAK_SRC_DIR}/src/emojidata.c
  ${UNIBREAK_SRC_DIR}/src/emojidef.c
  ${UNIBREAK_SRC_DIR}/src/graphemebreak.c
  ${UNIBREAK_SRC_DIR}/src/graphemebreakdata.c
  ${UNIBREAK_SRC_DIR}/src/indicconjunctbreakdata.c
  ${UNIBREAK_SRC_DIR}/src/linebreak.c
  ${UNIBREAK_SRC_DIR}/src/linebreakdata.c
  ${UNIBREAK_SRC_DIR}/src/linebreakdef.c
  ${UNIBREAK_SRC_DIR}/src/unibreakbase.c
  ${UNIBREAK_SRC_DIR}/src/unibreakdef.c
  ${UNIBREAK_SRC_DIR}/src/wordbreak.c
  ${UNIBREAK_SRC_DIR}/src/wordbreakdata.c
)

set(UNIBREAK_HEADERS
  ${UNIBREAK_SRC_DIR}/src/eastasianwidthdef.h
  ${UNIBREAK_SRC_DIR}/src/emojidef.h
  ${UNIBREAK_SRC_DIR}/src/graphemebreak.h
  ${UNIBREAK_SRC_DIR}/src/graphemebreakdef.h
  ${UNIBREAK_SRC_DIR}/src/indicconjunctbreakdef.h
  ${UNIBREAK_SRC_DIR}/src/linebreak.h
  ${UNIBREAK_SRC_DIR}/src/linebreakdef.h
  ${UNIBREAK_SRC_DIR}/src/unibreakbase.h
  ${UNIBREAK_SRC_DIR}/src/unibreakdef.h
  ${UNIBREAK_SRC_DIR}/src/wordbreak.h
  ${UNIBREAK_SRC_DIR}/src/wordbreakdef.h
)

add_library(libunibreak ${UNIBREAK_SOURCES} ${UNIBREAK_HEADERS})
target_include_directories(libunibreak PUBLIC
  $<BUILD_INTERFACE:${UNIBREAK_SRC_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(libunibreak PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
set_target_properties(libunibreak PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)


install(TARGETS libunibreak
  EXPORT libunibreakConfig
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${UNIBREAK_HEADERS} DESTINATION include)
install(EXPORT libunibreakConfig
  NAMESPACE libunibreak::
  DESTINATION lib/cmake/libunibreak
)
