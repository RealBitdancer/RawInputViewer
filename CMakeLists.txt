####################################################################################################
#
#    RawInputViewer - A utility to test, visualize, and map WM_INPUT messages.
#
#    Copyright (c) 2025-2026 Bitdancer (github.com/RealBitdancer)
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
        "src/res/StatusToolBar.bmp"
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
        "src/res/StatusToolBar.bmp"
        "src/res/ToolBar.bmp"
)

if(MSVC)
    # rc.exe writes ${PROJECT_NAME}.aps to its working directory (not beside the .rc).
    # Run RC from the per-config build dir while compiling the source-tree .rc.
    set(_res_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/res")
    if(CMAKE_CONFIGURATION_TYPES)
        set(_rc_workdir "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")
    else()
        set(_rc_workdir "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
    set(_rc_out "${_rc_workdir}/${PROJECT_NAME}.res")

    add_custom_command(
        OUTPUT "${_rc_out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_rc_workdir}"
        COMMAND ${CMAKE_COMMAND} -E chdir "${_rc_workdir}"
            ${CMAKE_RC_COMPILER} /nologo
            "/I${_res_dir}"
            /fo ${PROJECT_NAME}.res
            "${_res_dir}/${PROJECT_NAME}.rc"
        DEPENDS
            "${_res_dir}/${PROJECT_NAME}.rc"
            "${_res_dir}/resource.h"
            "${_res_dir}/${PROJECT_NAME}.ico"
            "${_res_dir}/ScanCodeMapping.txt"
            "${_res_dir}/VirtualKeyMapping.txt"
            "${_res_dir}/ListView.bmp"
            "${_res_dir}/StatusToolBar.bmp"
            "${_res_dir}/ToolBar.bmp"
        COMMENT "Compiling Windows resources (${PROJECT_NAME}.aps -> build dir)"
        VERBATIM
    )
    add_custom_target(${PROJECT_NAME}Res DEPENDS "${_rc_out}")
    add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}Res)

    # Visual Studio does not always pass a generated .res from target_sources to the
    # linker; pass it explicitly (same as dropping ${PROJECT_NAME}.res on Linker inputs).
    target_link_options(${PROJECT_NAME} PRIVATE "$<SHELL_PATH:${_rc_out}>")

    set_source_files_properties("src/res/${PROJECT_NAME}.rc" PROPERTIES VS_SETTINGS "ExcludedFromBuild=true")
else()
    set_source_files_properties("src/res/${PROJECT_NAME}.rc" PROPERTIES LANGUAGE RC)
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DPI_AWARE "PerMonitor")

target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_options(${PROJECT_NAME} PRIVATE /W3)
target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/WX>)

target_include_directories(${PROJECT_NAME} PRIVATE src/res)
target_link_libraries(${PROJECT_NAME} PRIVATE user32 comctl32 Version)
