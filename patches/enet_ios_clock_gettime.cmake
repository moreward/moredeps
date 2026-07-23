# Fix enet for iOS: the clock_gettime fallback for old macOS (< 10.12) also
# activates on iOS because __MAC_OS_X_VERSION_MIN_REQUIRED is undefined there.
# iOS 10+ has clock_gettime natively; skip the fallback on iOS.
file(READ "${SOURCE_DIR}/include/enet.h" _content)

string(REPLACE
  "#elif __APPLE__ && __MAC_OS_X_VERSION_MIN_REQUIRED < 101200"
  "#elif __APPLE__ && !TARGET_OS_IPHONE && __MAC_OS_X_VERSION_MIN_REQUIRED < 101200"
  _content "${_content}")

file(WRITE "${SOURCE_DIR}/include/enet.h" "${_content}")
