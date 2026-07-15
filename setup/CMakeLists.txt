qt_add_executable(arachnel_setup
    src/main.cpp
    src/payload_footer.cpp
    src/payload_footer_io.cpp
    src/win_container_io.cpp
    src/self_extractor.cpp
    src/setup_backend.cpp
    src/setup_translation.cpp
    src/win_install_registry.cpp
)

set_target_properties(arachnel_setup PROPERTIES
    OUTPUT_NAME arachnel_setup
    WIN32_EXECUTABLE TRUE
)

set(_setup_icon_ico "${CMAKE_SOURCE_DIR}/resources/icons/arachnel.ico")

function(_arachnel_setup_admin_manifest target)
    if(MSVC)
        target_link_options(${target} PRIVATE
            "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
    else()
        target_sources(${target} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/arachnel_admin.manifest.rc")
    endif()
endfunction()
if(EXISTS "${_setup_icon_ico}")
    set(_setup_icon_rc "${CMAKE_CURRENT_BINARY_DIR}/arachnel_setup_icon.rc")
    file(WRITE "${_setup_icon_rc}" "IDI_ICON1 ICON \"${_setup_icon_ico}\"\n")
    target_sources(arachnel_setup PRIVATE "${_setup_icon_rc}")
endif()

_arachnel_setup_admin_manifest(arachnel_setup)

qt_add_resources(arachnel_setup "arachnel_setup_icons"
    PREFIX "/icons"
    FILES
        ../resources/icons/png/256.png
        ../resources/icons/png/32.png
)

file(GLOB_RECURSE ARACHNEL_SETUP_QML_FILES CONFIGURE_DEPENDS
     RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
     "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml")

set_source_files_properties(
    qml/theme/Appearance.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

set(_setup_qml_module_args
    URI ArachnelSetup
    QML_FILES
        ${ARACHNEL_SETUP_QML_FILES}
    DEPENDENCIES
        Qcm.Material
        QtQuick.Effects
)
if(ARACHNEL_FAST_BUILD)
    list(APPEND _setup_qml_module_args NO_CACHEGEN)
endif()

qt_add_qml_module(arachnel_setup ${_setup_qml_module_args})

find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(arachnel_setup
    TS_FILES
        translations/arachnel_setup_en.ts
        translations/arachnel_setup_ru.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete
)

add_dependencies(arachnel_setup qml_materialplugin)

target_compile_definitions(arachnel_setup PRIVATE
    "QT_QML_MATERIAL_IMPORT_PATH=\"qml_modules\""
)

target_link_libraries(arachnel_setup
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Concurrent
        Qt6::Qml
        Qt6::Quick
        qml_material::qml_material
        ole32
        shell32
        advapi32
)

qt_import_qml_plugins(arachnel_setup)

add_executable(arachnel_setup_launcher WIN32
    src/stub_main.cpp
    src/stub_extractor.cpp
    src/payload_footer_io.cpp
    src/win_container_io.cpp
)

set_target_properties(arachnel_setup_launcher PROPERTIES
    OUTPUT_NAME arachnel_setup_launcher
    WIN32_EXECUTABLE TRUE
)

if(EXISTS "${_setup_icon_ico}")
    set(_setup_launcher_icon_rc "${CMAKE_CURRENT_BINARY_DIR}/arachnel_setup_launcher_icon.rc")
    file(WRITE "${_setup_launcher_icon_rc}" "IDI_ICON1 ICON \"${_setup_icon_ico}\"\n")
    target_sources(arachnel_setup_launcher PRIVATE "${_setup_launcher_icon_rc}")
endif()

_arachnel_setup_admin_manifest(arachnel_setup_launcher)

target_link_libraries(arachnel_setup_launcher PRIVATE shell32 ole32)

add_executable(arachnel_uninstall WIN32
    src/uninstall_main.cpp
)

set_target_properties(arachnel_uninstall PROPERTIES
    OUTPUT_NAME uninstall
    WIN32_EXECUTABLE TRUE
)

if(EXISTS "${_setup_icon_ico}")
    set(_setup_uninstall_icon_rc "${CMAKE_CURRENT_BINARY_DIR}/arachnel_uninstall_icon.rc")
    file(WRITE "${_setup_uninstall_icon_rc}" "IDI_ICON1 ICON \"${_setup_icon_ico}\"\n")
    target_sources(arachnel_uninstall PRIVATE "${_setup_uninstall_icon_rc}")
endif()

_arachnel_setup_admin_manifest(arachnel_uninstall)

target_link_libraries(arachnel_uninstall PRIVATE shell32 ole32 advapi32)

function(_arachnel_setup_mingw_static_runtime target)
    if(NOT MINGW)
        return()
    endif()
    target_link_options(${target} PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,-Bstatic
        -lwinpthread
        -Wl,-Bdynamic
    )
endfunction()

_arachnel_setup_mingw_static_runtime(arachnel_setup)
_arachnel_setup_mingw_static_runtime(arachnel_uninstall)

if(MINGW)
    target_link_options(arachnel_setup_launcher PRIVATE -static)
endif()
