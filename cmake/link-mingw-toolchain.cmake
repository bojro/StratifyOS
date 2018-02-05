
# the name of the target operating system

message(STATUS "Set GCC toolchaing to i686-w64-mingw32")
set(TOOLCHAIN_HOST i686-w64-mingw32 CACHE INTERNAL "" FORCE)
set(TOOLCHAIN_DIR "C:/Qt-5.9/Tools/mingw530_32")
set(TOOLCHAIN_EXEC_SUFFIX .exe)

set(SDK_DIR C:/StratifyLabs-SDK/Tools/gcc)

include(${CMAKE_CURRENT_LIST_DIR}/sos-gcc-toolchain.cmake)

set(TOOLCHAIN_C_FLAGS "" CACHE INTERNAL "common c compiler flags")
set(TOOLCHAIN_CXX_FLAGS "" CACHE INTERNAL "common c compiler flags")

set(CMAKE_INSTALL_PREFIX ${SDK_DIR} CACHE INTERNAL "install prefix")
include_directories(SYSTEM ${SDK_DIR}/include)

set(CMAKE_VERBOSE_MAKEFILE TRUE CACHE INTERNAL "verbose make")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_C_FLAGS "${TOOLCHAIN_C_FLAGS} -D__win32 -mno-ms-bitfields" CACHE INTERNAL "c compiler flags release")
set(CMAKE_CXX_FLAGS "${TOOLCHAIN_C_FLAGS} ${TOOLCHAIN_CXX_FLAGS}" CACHE INTERNAL "cxx compiler flags release")