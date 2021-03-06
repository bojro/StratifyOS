
set(SOS_START_ADDRESS ${SOS_KERNEL_START_ADDRESS})
set(SOS_SOURCELIST ${SOS_KERNEL_SOURCELIST})
set(SOS_CONFIG release)
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-bsp.cmake)
set(SOS_CONFIG debug)
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-bsp.cmake)

set(SOS_SOURCELIST ${SOS_BOOT_SOURCELIST})
set(SOS_START_ADDRESS ${SOS_BOOT_START_ADDRESS})
set(SOS_CONFIG release_boot)
list(APPEND SOS_DEFINITIONS __KERNEL_START_ADDRESS=${SOS_KERNEL_START_ADDRESS})
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-bsp.cmake)
set(SOS_CONFIG debug_boot)
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-bsp.cmake)
