# Shared sources and headers for Arachnel source plugins (out-of-tree builds).
# Usage from a plugin repo:
#   set(ARACHNEL_SDK_DIR "/path/to/Arachnel")
#   include(${ARACHNEL_SDK_DIR}/cmake/ArachnelPluginSdk.cmake)
#   arachnel_plugin_sdk_init(${ARACHNEL_SDK_DIR})
#   target_link_libraries(my_plugin PRIVATE arachnel_plugin_sdk)

function(arachnel_plugin_sdk_init ARACHNEL_ROOT)
    if(TARGET arachnel_plugin_sdk)
        return()
    endif()

    if(NOT EXISTS "${ARACHNEL_ROOT}/src/core/plugin_interface.h")
        message(FATAL_ERROR "arachnel_plugin_sdk: invalid ARACHNEL_SDK_DIR: ${ARACHNEL_ROOT}")
    endif()

    set(_ARACHNEL_PLUGIN_SDK_SOURCES
        ${ARACHNEL_ROOT}/src/core/catalog_parser.cpp
        ${ARACHNEL_ROOT}/src/core/catalog_types.cpp
        ${ARACHNEL_ROOT}/src/core/install_kind.cpp
        ${ARACHNEL_ROOT}/src/core/install_analysis.cpp
        ${ARACHNEL_ROOT}/src/core/install_heuristics.cpp
        ${ARACHNEL_ROOT}/src/core/file_utils.cpp
        ${ARACHNEL_ROOT}/src/core/windows_runner.cpp
        ${ARACHNEL_ROOT}/src/core/proton_manager.cpp
        ${ARACHNEL_ROOT}/src/core/plugin_api.cpp
    )

    add_library(arachnel_plugin_sdk STATIC ${_ARACHNEL_PLUGIN_SDK_SOURCES})
    target_compile_definitions(arachnel_plugin_sdk PUBLIC ARACHNEL_PLUGIN_BUILD)
    target_include_directories(arachnel_plugin_sdk
        PUBLIC
            ${ARACHNEL_ROOT}/src/core
    )
    target_link_libraries(arachnel_plugin_sdk PUBLIC Qt6::Core Qt6::Network)
    if(WIN32)
        target_link_libraries(arachnel_plugin_sdk PUBLIC shell32)
    endif()
    set_target_properties(arachnel_plugin_sdk PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()

function(arachnel_stage_plugin_bundle TARGET_NAME PLUGIN_ID STAGING_DIR)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${STAGING_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${TARGET_NAME}>"
            "${STAGING_DIR}/$<TARGET_FILE_NAME:${TARGET_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
            "${STAGING_DIR}/plugin.json"
        COMMENT "Stage ${PLUGIN_ID} plugin bundle"
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/games-arachnel.json")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/games-arachnel.json"
                "${STAGING_DIR}/games-arachnel.json"
        )
    endif()
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/linux")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/linux"
                "${STAGING_DIR}/linux"
        )
    endif()
endfunction()

function(arachnel_package_plugin_arach TARGET_NAME PLUGIN_ID STAGING_DIR ARACH_PATH)
    get_filename_component(_arach_parent "${ARACH_PATH}" DIRECTORY)
    get_filename_component(_staging_parent "${STAGING_DIR}" DIRECTORY)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_arach_parent}"
        COMMENT "Package ${PLUGIN_ID}.arach"
    )
    if(WIN32)
        set(_arach_zip "${_arach_parent}/${PLUGIN_ID}.zip")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
                "if (Test-Path -LiteralPath '${ARACH_PATH}') { Remove-Item -LiteralPath '${ARACH_PATH}' -Force }; if (Test-Path -LiteralPath '${_arach_zip}') { Remove-Item -LiteralPath '${_arach_zip}' -Force }; Compress-Archive -Path '${STAGING_DIR}' -DestinationPath '${_arach_zip}' -Force; Move-Item -LiteralPath '${_arach_zip}' -Destination '${ARACH_PATH}' -Force"
        )
    else()
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E chdir "${_staging_parent}"
                ${CMAKE_COMMAND} -E tar cf "${ARACH_PATH}" --format=zip "${PLUGIN_ID}"
        )
    endif()
endfunction()
