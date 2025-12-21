from comp_backend import *
import pathlib

CLFLAGS="/GS- /GL /O1 /favor:AMD64 /nologo"
LINKFLAGS="/LTCG"

BUILD_DIR = "build"
BIN_DIR = "bin"

CLFLAGS_DBG = f"-g -Og"
LINKFLAGS_DBG = f"-g"

BUILD_DIR_DBG = "build-dbg"
BIN_DIR_DBG = "bin-dbg"

BUILD_DIR_WEB = "build-web"
BIN_DIR_WEB = "bin-web"


WORKDIR = pathlib.Path(__file__).parent.resolve()

add_backend("msvc", "Msvc", BUILD_DIR, BIN_DIR, WORKDIR, CLFLAGS, LINKFLAGS)
add_backend("mingw", "Mingw", BUILD_DIR_DBG, BIN_DIR_DBG, WORKDIR, CLFLAGS_DBG, LINKFLAGS_DBG)
add_backend("gcc", "Gcc", "build-gcc", "bin-gcc", WORKDIR, "-g -O0", "-g")

set_backend("Msvc")

def main():
    sdl3 = find_package("SDL3")
    sdl3_image = find_package("SDL3_image")

    if backend().name == "gcc":
        link = "-Wl,-rpath,'$ORIGIN'"
    else:
        link = None

    Executable("main", "src/main.c", "src/dynamic_string.c", 
               packages=[sdl3, sdl3_image], extra_link_flags=link)
    CopyToBin(*sdl3.dlls, *sdl3_image.dlls)

    build(__file__)


if __name__ == "__main__":
    main()
