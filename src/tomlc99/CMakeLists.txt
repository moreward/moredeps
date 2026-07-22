cmake_minimum_required(VERSION 3.10)
project(tomlc99 C)

set(TOML_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/tomlc99")

add_library(tomlc99 STATIC
    "${TOML_SOURCE_DIR}/toml.c"
)
target_include_directories(tomlc99 PUBLIC "${TOML_SOURCE_DIR}")
target_compile_definitions(tomlc99 PRIVATE _POSIX_C_SOURCE=200809L)

install(TARGETS tomlc99 ARCHIVE DESTINATION lib)
install(FILES "${TOML_SOURCE_DIR}/toml.h" DESTINATION include)
