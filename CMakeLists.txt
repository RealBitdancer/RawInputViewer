####################################################################################################
#
#    RawInputViewer - A utility to test, visualize, and map WM_INPUT messages.
#
#    Copyright (c) 2025 by Bitdancer (@RealBitdancer)
#
#    Licensed under the MIT License. See LICENSE file in the repository for details.
#
#    Source: https://github.com/RealBitdancer/RawInputViewer
#
####################################################################################################

cmake_minimum_required(VERSION 3.20)

project(RawInputViewer LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Limited configurations" FORCE)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

add_executable(${PROJECT_NAME} WIN32)

target_sources(${PROJECT_NAME}
    PUBLIC
        "src/${PROJECT_NAME}.cpp"

        # Header files are here as workaround to ensure folder
        # "Header Files" for VS2022 project files is generated.
        "src/${PROJECT_NAME}.hpp"
        "src/res/resource.h"

        # Resource files are here as workaround to ensure
        # source_group "Resource Files" is generated.
        "src/res/${PROJECT_NAME}.rc"
        "src/res/${PROJECT_NAME}.ico"
        "src/res/${PROJECT_NAME}.manifest"
        "src/res/ScanCodeMapping.txt"
        "src/res/VirtualKeyMapping.txt"
        "src/res/ListView.bmp"
        "src/res/ToolBar.bmp"

        # There seems to be no sensible workaround for having ${PROJECT_NAME}.manifest
        # show up in the Solution Explorer, which is annoying to say the least.
)

source_group("Resource Files"
    FILES
        "src/res/${PROJECT_NAME}.rc"
        "src/res/${PROJECT_NAME}.ico"
        "src/res/ScanCodeMapping.txt"
        "src/res/VirtualKeyMapping.txt"
        "src/res/ListView.bmp"
        "src/res/ToolBar.bmp"
)

set_source_files_properties("src/res/${PROJECT_NAME}.rc" PROPERTIES LANGUAGE RC)

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DPI_AWARE "PerMonitor")

target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_options(${PROJECT_NAME} PRIVATE /W3)
target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/WX>)

target_include_directories(${PROJECT_NAME} PRIVATE src/res)
target_link_libraries(${PROJECT_NAME} PRIVATE user32 comctl32 Version)
