cmake_minimum_required(VERSION 3.25)
project(skribidi LANGUAGES C CXX)

set(SKRIBIDI_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/skribidi")
set(INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Link against the dependencies already installed by the top-level build.
# We use find_library so the same wrapper works for static and shared builds
# (the file extension and import-lib location differ by platform).
set(_SEARCH_PATHS "${INSTALL_PREFIX}/lib")
if(WIN32 AND BUILD_SHARED_LIBS)
  # During the shared pass the import libraries are staged directly in lib/
  # before the build_all.sh merge moves them to lib/import/.
  list(APPEND _SEARCH_PATHS "${INSTALL_PREFIX}/lib/import")
endif()

find_library(_HARFBUZZ_LIB NAMES harfbuzz PATHS ${_SEARCH_PATHS} NO_DEFAULT_PATH)
find_library(_SHEENBIDI_LIB NAMES SheenBidi PATHS ${_SEARCH_PATHS} NO_DEFAULT_PATH)
find_library(_LIBUNIBREAK_LIB NAMES libunibreak PATHS ${_SEARCH_PATHS} NO_DEFAULT_PATH)
find_library(_BUDOUXC_LIB NAMES budouxc PATHS ${_SEARCH_PATHS} NO_DEFAULT_PATH)
find_library(_FREETYPE_LIB NAMES freetype PATHS ${_SEARCH_PATHS} NO_DEFAULT_PATH)

if(NOT _HARFBUZZ_LIB)
  message(FATAL_ERROR "skribidi: harfbuzz library not found in ${INSTALL_PREFIX}/lib")
endif()
if(NOT _SHEENBIDI_LIB)
  message(FATAL_ERROR "skribidi: SheenBidi library not found in ${INSTALL_PREFIX}/lib")
endif()
if(NOT _LIBUNIBREAK_LIB)
  message(FATAL_ERROR "skribidi: libunibreak library not found in ${INSTALL_PREFIX}/lib")
endif()
if(NOT _BUDOUXC_LIB)
  message(FATAL_ERROR "skribidi: budouxc library not found in ${INSTALL_PREFIX}/lib")
endif()
if(NOT _FREETYPE_LIB)
  message(FATAL_ERROR "skribidi: freetype library not found in ${INSTALL_PREFIX}/lib")
endif()

add_library(skribidi
  ${SKRIBIDI_SRC_DIR}/src/skb_attributes.c
  ${SKRIBIDI_SRC_DIR}/src/skb_attribute_collection.c
  ${SKRIBIDI_SRC_DIR}/src/skb_canvas.c
  ${SKRIBIDI_SRC_DIR}/src/skb_common.c
  ${SKRIBIDI_SRC_DIR}/src/skb_editor.c
  ${SKRIBIDI_SRC_DIR}/src/skb_editor_rules.c
  ${SKRIBIDI_SRC_DIR}/src/skb_font_collection.c
  ${SKRIBIDI_SRC_DIR}/src/skb_icon_collection.c
  ${SKRIBIDI_SRC_DIR}/src/skb_image_atlas.c
  ${SKRIBIDI_SRC_DIR}/src/skb_layout.c
  ${SKRIBIDI_SRC_DIR}/src/skb_layout_cache.c
  ${SKRIBIDI_SRC_DIR}/src/skb_rasterizer.c
  ${SKRIBIDI_SRC_DIR}/src/skb_rich_layout.c
  ${SKRIBIDI_SRC_DIR}/src/skb_rich_text.c
  ${SKRIBIDI_SRC_DIR}/src/skb_text.c

  ${SKRIBIDI_SRC_DIR}/include/skb_attributes.h
  ${SKRIBIDI_SRC_DIR}/include/skb_attribute_collection.h
  ${SKRIBIDI_SRC_DIR}/include/skb_canvas.h
  ${SKRIBIDI_SRC_DIR}/include/skb_common.h
  ${SKRIBIDI_SRC_DIR}/include/skb_editor.h
  ${SKRIBIDI_SRC_DIR}/include/skb_editor_rules.h
  ${SKRIBIDI_SRC_DIR}/include/skb_font_collection.h
  ${SKRIBIDI_SRC_DIR}/include/skb_icon_collection.h
  ${SKRIBIDI_SRC_DIR}/include/skb_image_atlas.h
  ${SKRIBIDI_SRC_DIR}/include/skb_layout.h
  ${SKRIBIDI_SRC_DIR}/include/skb_layout_cache.h
  ${SKRIBIDI_SRC_DIR}/include/skb_rasterizer.h
  ${SKRIBIDI_SRC_DIR}/include/skb_rich_layout.h
  ${SKRIBIDI_SRC_DIR}/include/skb_rich_text.h
  ${SKRIBIDI_SRC_DIR}/include/skb_text.h
)
set_target_properties(skribidi PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)


target_include_directories(skribidi PUBLIC
  $<BUILD_INTERFACE:${SKRIBIDI_SRC_DIR}/include>
  $<BUILD_INTERFACE:${SKRIBIDI_SRC_DIR}/src>
  $<BUILD_INTERFACE:${INSTALL_PREFIX}/include/harfbuzz>
  $<BUILD_INTERFACE:${INSTALL_PREFIX}/include/SheenBidi>
  $<BUILD_INTERFACE:${INSTALL_PREFIX}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(skribidi PUBLIC
  ${_HARFBUZZ_LIB}
  ${_SHEENBIDI_LIB}
  ${_LIBUNIBREAK_LIB}
  ${_BUDOUXC_LIB}
  ${_FREETYPE_LIB}
)

if(NOT WIN32)
  target_link_libraries(skribidi PUBLIC m)
endif()

set_target_properties(skribidi PROPERTIES
  C_STANDARD 17
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
  POSITION_INDEPENDENT_CODE ON
  CXX_STANDARD 17
)

install(TARGETS skribidi
  EXPORT skribidiConfig
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(FILES
  ${SKRIBIDI_SRC_DIR}/include/skb_attributes.h
  ${SKRIBIDI_SRC_DIR}/include/skb_attribute_collection.h
  ${SKRIBIDI_SRC_DIR}/include/skb_canvas.h
  ${SKRIBIDI_SRC_DIR}/include/skb_common.h
  ${SKRIBIDI_SRC_DIR}/include/skb_editor.h
  ${SKRIBIDI_SRC_DIR}/include/skb_editor_rules.h
  ${SKRIBIDI_SRC_DIR}/include/skb_font_collection.h
  ${SKRIBIDI_SRC_DIR}/include/skb_icon_collection.h
  ${SKRIBIDI_SRC_DIR}/include/skb_image_atlas.h
  ${SKRIBIDI_SRC_DIR}/include/skb_layout.h
  ${SKRIBIDI_SRC_DIR}/include/skb_layout_cache.h
  ${SKRIBIDI_SRC_DIR}/include/skb_rasterizer.h
  ${SKRIBIDI_SRC_DIR}/include/skb_rich_layout.h
  ${SKRIBIDI_SRC_DIR}/include/skb_rich_text.h
  ${SKRIBIDI_SRC_DIR}/include/skb_text.h
  DESTINATION include
)

install(EXPORT skribidiConfig
  NAMESPACE skribidi::
  DESTINATION lib/cmake/skribidi
)
