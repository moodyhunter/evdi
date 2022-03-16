cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
# Ref:
# https://musteresel.github.io/posts/2020/02/cmake-template-linux-kernel-module.html
# https://github.com/tud-zih-energy/x86_adapt/blob/master/CMakeLists.txt
# https://gitlab.com/phip1611/cmake-kernel-module/-/blob/ubuntu-clion-ide/CMakeLists.txt
# https://github.com/enginning/cmake-kernel-module/blob/main/CMakeLists.txt

project(evdi VERSION 0.0.1 LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(MODULE_NAME evdi)

# Find kernel headers
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(KernelBuild)

set(LIST_SRC
    evdi_connector.c
    evdi_cursor.c
    evdi_debug.c
    evdi_drm_drv.c
    evdi_encoder.c
    evdi_fb.c
    evdi_gem.c
    evdi_i2c.c
    evdi_modeset.c
    evdi_painter.c
    evdi_params.c
    evdi_platform_dev.c
    evdi_platform_drv.c
    evdi_sysfs.c
    evdi_ioc32.c
)

add_library(evdi_lib SHARED library/evdi_lib.c)
target_include_directories(evdi_lib PUBLIC ${CMAKE_CURRENT_LIST_DIR}/module/)
target_include_directories(evdi_lib PUBLIC ${CMAKE_CURRENT_LIST_DIR}/library/)

# Add include directories to Kbuild EXTRA_CFLAGS
set(MODULE_INCLUDE
    ${KERNEL_BUILD_DIR}/arch/x86/include 
    ${KERNEL_BUILD_DIR}/arch/x86/include/generated 
    ${CMAKE_BINARY_DIR}/module
    ${CMAKE_CURRENT_LIST_DIR}/module)

foreach (include_dir ${MODULE_INCLUDE})
    list(APPEND KBUILD_INCLUDE "-I${include_dir}")
endforeach ()

string(REPLACE ";" " " KBUILD_INCLUDE_STRING "${KBUILD_INCLUDE}")
string(REPLACE ";" " " LIST_SRC_STRING "${LIST_SRC}")

configure_file(module/Kbuild.in module/Kbuild @ONLY)
configure_file(module/config.h.in module/config.h @ONLY)

set(make_cmd ${CMAKE_MAKE_PROGRAM} -C ${KERNEL_BUILD_DIR} M=${CMAKE_BINARY_DIR}/module)

foreach(src ${LIST_SRC})
    configure_file(module/${src} module/${src} COPYONLY)
endforeach()

add_custom_command(OUTPUT ${MODULE_NAME}.ko
        COMMAND ${make_cmd} V=1 modules
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/module
        DEPENDS ${CMAKE_BINARY_DIR}/module/Kbuild
        VERBATIM)

add_custom_target(module DEPENDS ${MODULE_NAME}.ko)
add_custom_target(module_clean COMMAND ${make_cmd} clean)
