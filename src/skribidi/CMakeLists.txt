cmake_minimum_required(VERSION 3.25)
project(skribidi LANGUAGES C CXX)

set(SKRIBIDI_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/skribidi")
set(INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Directly reference the libraries installed by this project instead of using
# find_package, because some upstream CMake configs (e.g. harfbuzz 11.0.0)
# encode broken absolute paths or require targets like Threads::Threads that
# are not automatically found in this isolated ExternalProject build.
add_library(skribidi STATIC
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

target_include_directories(skribidi PUBLIC
  $<BUILD_INTERFACE:${SKRIBIDI_SRC_DIR}/include>
  $<BUILD_INTERFACE:${SKRIBIDI_SRC_DIR}/src>
  $<BUILD_INTERFACE:${INSTALL_PREFIX}/include/harfbuzz>
  $<BUILD_INTERFACE:${INSTALL_PREFIX}/include/SheenBidi>
  $<BUILD_INTERFACE:${INSTALL_PREFIX}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(skribidi PUBLIC
  ${INSTALL_PREFIX}/lib/libharfbuzz.a
  ${INSTALL_PREFIX}/lib/libSheenBidi.a
  ${INSTALL_PREFIX}/lib/liblibunibreak.a
  ${INSTALL_PREFIX}/lib/libbudouxc.a
  ${INSTALL_PREFIX}/lib/libfreetype.a
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
