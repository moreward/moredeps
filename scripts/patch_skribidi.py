# Patch skribidi's CMakeLists to disable global warning-as-error, which
# breaks fetched dependencies like harfbuzz on Emscripten.
from pathlib import Path
import sys

src_copy = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / "deps" / "skribidi"
path = src_copy / "CMakeLists.txt"
text = path.read_text(encoding="utf-8-sig")
text = text.replace("set(CMAKE_COMPILE_WARNING_AS_ERROR ON)", "# set(CMAKE_COMPILE_WARNING_AS_ERROR ON)")
path.write_text(text, encoding="utf-8")
print(f"Patched {path} to disable CMAKE_COMPILE_WARNING_AS_ERROR")
