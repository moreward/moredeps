# Patch miniaudio for iOS: compile as Objective-C so CoreAudio → Foundation
# headers don't fail in C mode (iOS 26+ SDK behavior).
file(READ "${SOURCE_DIR}/CMakeLists.txt" _content)

# Add OBJC to the project languages on Apple targets.
string(REPLACE
  "project(miniaudio VERSION \${MINIAUDIO_VERSION})"
  "if(APPLE)\n    project(miniaudio VERSION \${MINIAUDIO_VERSION} LANGUAGES C OBJC)\nelse()\n    project(miniaudio VERSION \${MINIAUDIO_VERSION})\nendif()"
  _content "${_content}")

# Compile the main source as ObjC on Apple targets.
string(REPLACE
  "add_library(miniaudio\n    miniaudio.c"
  "add_library(miniaudio\n    miniaudio.c\n)\nif(APPLE)\n    set_source_files_properties(miniaudio.c PROPERTIES COMPILE_FLAGS \"-x objective-c\")\nendif()\n\n# dummy"
  _content "${_content}")

file(WRITE "${SOURCE_DIR}/CMakeLists.txt" "${_content}")
