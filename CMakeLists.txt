cmake_minimum_required(VERSION 3.20)
project(arachnel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Network
    Qml
    Quick
    QuickControls2
    ShaderTools
    QuickPrivate
)
if(UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBTORRENT REQUIRED IMPORTED_TARGET libtorrent-rasterbar)

qt_standard_project_setup(REQUIRES 6.8)

include(FetchContent)
FetchContent_Declare(
    qml_material
    GIT_REPOSITORY https://github.com/hypengw/QmlMaterial.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(qml_material)

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

qt_add_executable(arachnel_app
    src/main.cpp
    src/core/install_kind.cpp
    src/core/job_kind.cpp
    src/core/catalog_types.cpp
    src/core/library_model.cpp
    src/core/source_plugin_model.cpp
    src/core/catalog_model.cpp
    src/core/job_model.cpp
    src/core/settings_store.cpp
    src/core/library_store.cpp
    src/core/catalog_feed_loader.cpp
    src/core/game_metadata_service.cpp
    src/core/torrent_session.cpp
    src/core/job_orchestrator.cpp
    src/core/core_controller.cpp
)

file(GLOB_RECURSE ARACHNEL_QML_FILES CONFIGURE_DEPENDS
     RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
     "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml")

set_source_files_properties(
    qml/theme/Appearance.qml
    qml/theme/AccentColors.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(arachnel_app
    URI arachnel
    QML_FILES
        ${ARACHNEL_QML_FILES}
    DEPENDENCIES
        Qcm.Material
        QtQuick.Effects
)

add_dependencies(arachnel_app qml_materialplugin)

target_compile_definitions(arachnel_app PRIVATE
    "QT_QML_MATERIAL_IMPORT_PATH=\"${CMAKE_BINARY_DIR}/qml_modules\""
)

target_link_libraries(arachnel_app
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        PkgConfig::LIBTORRENT
        qml_material::qml_material
)
