cmake_minimum_required(VERSION 3.20)
project(arachnel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED ENV{ARACHNEL_VERSION} AND NOT "$ENV{ARACHNEL_VERSION}" STREQUAL "")
    set(ARACHNEL_VERSION "$ENV{ARACHNEL_VERSION}" CACHE STRING "Application version" FORCE)
elseif(NOT DEFINED ARACHNEL_VERSION)
    set(ARACHNEL_VERSION "dev" CACHE STRING "Application version")
endif()
message(STATUS "Arachnel version: ${ARACHNEL_VERSION}")

option(ARACHNEL_FAST_BUILD "Faster dev builds: skip QML cachegen" ON)
if(WIN32)
    option(ARACHNEL_LIBTORRENT_SHARED "Build libtorrent as DLL (much faster incremental links)" ON)
else()
    option(ARACHNEL_LIBTORRENT_SHARED "Build libtorrent as shared library" OFF)
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Network
    Qml
    Quick
    QuickControls2
    ShaderTools
    Concurrent
    Multimedia
)
if(NOT WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
endif()
if(UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
endif()

qt_standard_project_setup(REQUIRES 6.8)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArachnelLibtorrent.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArachnelTranslations.cmake)

# Optional Weblate helper — not part of the app build (needs Python).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    add_custom_target(arachnel_normalize_translations
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/normalize_translations.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Normalize translations/*.ts for Weblate"
    )
endif()

include(FetchContent)
FetchContent_Declare(
    qml_material
    GIT_REPOSITORY https://github.com/hypengw/QmlMaterial.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
    PATCH_COMMAND ${CMAKE_COMMAND} -DSRC=<SOURCE_DIR> -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch-qml-material-win.cmake
)
FetchContent_MakeAvailable(qml_material)

if(WIN32 AND EXISTS "${qml_material_SOURCE_DIR}/CMakeLists.txt")
    file(READ "${qml_material_SOURCE_DIR}/CMakeLists.txt" _qm_cmake)
    if(_qm_cmake MATCHES "SHADER_OPT BATCHABLE OPTIMIZED")
        string(REPLACE
            "set(SHADER_OPT BATCHABLE OPTIMIZED)"
            "set(SHADER_OPT BATCHABLE)"
            _qm_cmake "${_qm_cmake}")
        file(WRITE "${qml_material_SOURCE_DIR}/CMakeLists.txt" "${_qm_cmake}")
        message(STATUS "QmlMaterial: disabled shader OPTIMIZED (spirv-opt / Vulkan SDK not required)")
    endif()
endif()

# QmlMaterial icon fonts are Git LFS objects — pull before qml_material compiles qrc.
set(_QM_FONT "${qml_material_SOURCE_DIR}/assets/MaterialSymbolsRounded.wght_400.opsz_24.fill_0.woff2")
if(EXISTS "${_QM_FONT}")
    file(READ "${_QM_FONT}" _QM_FONT_HEAD LIMIT 40)
    if(_QM_FONT_HEAD MATCHES "^version https://git-lfs")
        find_program(GIT_LFS_EXECUTABLE git-lfs)
        if(GIT_LFS_EXECUTABLE)
            execute_process(
                COMMAND ${GIT_LFS_EXECUTABLE} pull
                WORKING_DIRECTORY "${qml_material_SOURCE_DIR}"
                RESULT_VARIABLE _QM_LFS_RESULT
            )
            if(NOT _QM_LFS_RESULT EQUAL 0)
                message(WARNING "git lfs pull failed for QmlMaterial fonts; icons may be missing")
            endif()
        else()
            message(WARNING "git-lfs not found; Material icon fonts may be missing")
        endif()
    endif()
endif()
unset(_QM_FONT)
unset(_QM_FONT_HEAD)
unset(_QM_LFS_RESULT)

set(CMAKE_AUTOMOC ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt)

qt_add_executable(arachnel_app
    ${ARACHNEL_APP_SOURCES}
)

if(WIN32)
    set_target_properties(arachnel_app PROPERTIES WIN32_EXECUTABLE TRUE)

    set(_arachnel_icon_ico "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/arachnel.ico")
    if(EXISTS "${_arachnel_icon_ico}")
        set(_arachnel_icon_rc "${CMAKE_CURRENT_BINARY_DIR}/arachnel_app_icon.rc")
        file(WRITE "${_arachnel_icon_rc}" "IDI_ICON1 ICON \"${_arachnel_icon_ico}\"\n")
        target_sources(arachnel_app PRIVATE "${_arachnel_icon_rc}")
    endif()
endif()

qt_add_resources(arachnel_app "arachnel_app_icons"
    PREFIX "/icons"
    FILES
        resources/icons/png/16.png
        resources/icons/png/24.png
        resources/icons/png/32.png
        resources/icons/png/48.png
        resources/icons/png/64.png
        resources/icons/png/128.png
        resources/icons/png/256.png
        resources/icons/png/512.png
)

file(GLOB_RECURSE ARACHNEL_QML_FILES CONFIGURE_DEPENDS
     RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
     "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml")

set_source_files_properties(
    qml/theme/Appearance.qml
    qml/theme/AccentColors.qml
    qml/i18n/Messages.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

set(_arachnel_qml_module_args
    URI arachnel
    QML_FILES
        ${ARACHNEL_QML_FILES}
    DEPENDENCIES
        Qcm.Material
        QtQuick.Effects
)
if(ARACHNEL_FAST_BUILD)
    list(APPEND _arachnel_qml_module_args NO_CACHEGEN)
endif()

qt_add_qml_module(arachnel_app ${_arachnel_qml_module_args})

arachnel_setup_translations(arachnel_app)

add_dependencies(arachnel_app qml_materialplugin)

target_compile_definitions(arachnel_app PRIVATE
    "QT_QML_MATERIAL_IMPORT_PATH=\"qml_modules\""
    "ARACHNEL_VERSION=\"${ARACHNEL_VERSION}\""
)

target_include_directories(arachnel_app PRIVATE ${ARACHNEL_CORE_INCLUDE_DIRS})

target_link_libraries(arachnel_app
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Concurrent
        Qt6::Qml
        Qt6::Quick
        qml_material::qml_material
)

qt_import_qml_plugins(arachnel_app)

arachnel_setup_libtorrent(arachnel_app)

if(WIN32)
    target_link_libraries(arachnel_app PRIVATE ole32 shell32 dbghelp)
    add_subdirectory(setup)
else()
    target_link_libraries(arachnel_app PRIVATE Qt6::Widgets)
    target_link_options(arachnel_app PRIVATE -rdynamic)
endif()
