# scripts/apply_patch.cmake
# Idempotent helper used by ExternalProject's PATCH_COMMAND.
# Resets the tracked file to its upstream state and then applies a patch.
# This makes it safe to use the same source directory for both static and
# shared variants of an ExternalProject.

cmake_minimum_required(VERSION 3.10)

if(NOT PATCH_FILE OR NOT SOURCE_DIR)
  message(FATAL_ERROR "apply_patch.cmake requires PATCH_FILE and SOURCE_DIR")
endif()

message(STATUS "Applying patch ${PATCH_FILE} to ${SOURCE_DIR}")

# Reset tracked files so repeated applications (e.g. static then shared
# variant) do not conflict.
execute_process(
  COMMAND git -C "${SOURCE_DIR}" checkout -- .
  COMMAND_ERROR_IS_FATAL ANY
)

execute_process(
  COMMAND git -C "${SOURCE_DIR}" apply --ignore-whitespace "${PATCH_FILE}"
  COMMAND_ERROR_IS_FATAL ANY
)
