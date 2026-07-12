set(FREETP_PLUGIN_DIR "${CMAKE_SOURCE_DIR}/plugins/freetp")
set(FREETP_CATALOG "${FREETP_PLUGIN_DIR}/games-arachnel.json")
set(FREETP_CATALOG_SAMPLE "${FREETP_PLUGIN_DIR}/games.sample.json")
set(FREETP_CATALOG_URL
    "https://gitlab.com/BadKiko/freetp-hydra-link/-/raw/main/games-arachnel.json?ref_type=heads")

if(NOT EXISTS "${FREETP_CATALOG_SAMPLE}")
    message(FATAL_ERROR "freetp: missing games.sample.json")
endif()

if(NOT "$ENV{ARACHNEL_SKIP_FREETP_CATALOG_FETCH}" STREQUAL "1")
    message(STATUS "freetp: fetching games-arachnel.json ...")
    file(DOWNLOAD "${FREETP_CATALOG_URL}" "${FREETP_CATALOG}" STATUS _dl_status TIMEOUT 180)
    list(GET _dl_status 0 _dl_code)
    if(NOT _dl_code EQUAL 0)
        message(WARNING "freetp: could not download games-arachnel.json (using sample)")
        file(COPY "${FREETP_CATALOG_SAMPLE}" DESTINATION "${FREETP_PLUGIN_DIR}")
        file(RENAME "${FREETP_PLUGIN_DIR}/games.sample.json" "${FREETP_CATALOG}")
    endif()
elseif(NOT EXISTS "${FREETP_CATALOG}")
    file(COPY "${FREETP_CATALOG_SAMPLE}" DESTINATION "${FREETP_PLUGIN_DIR}")
    file(RENAME "${FREETP_PLUGIN_DIR}/games.sample.json" "${FREETP_CATALOG}")
endif()

add_library(freetp_plugin SHARED
    ${FREETP_PLUGIN_DIR}/src/freetp_plugin.cpp
    ${FREETP_PLUGIN_DIR}/src/archive_installer.cpp
    ${FREETP_PLUGIN_DIR}/src/installer_runner.cpp
    ${FREETP_PLUGIN_DIR}/src/plugin_entry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/catalog_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/catalog_types.cpp
    ${CMAKE_SOURCE_DIR}/src/core/install_kind.cpp
    ${CMAKE_SOURCE_DIR}/src/core/file_utils.cpp
)

target_compile_definitions(freetp_plugin PRIVATE ARACHNEL_PLUGIN_BUILD)
target_include_directories(freetp_plugin
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
        ${FREETP_PLUGIN_DIR}/src
)
target_link_libraries(freetp_plugin PRIVATE Qt6::Core Qt6::Network)

set_target_properties(freetp_plugin PROPERTIES
    OUTPUT_NAME "freetp_plugin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugin-build/freetp"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugin-build/freetp"
)
if(MINGW)
    set_target_properties(freetp_plugin PROPERTIES PREFIX "")
endif()

set(_PLUGIN_DEPLOY_DIR "${CMAKE_BINARY_DIR}/plugin-build/freetp")
set(_FREETP_ARACH "${CMAKE_BINARY_DIR}/dist/freetp.arach")
set(_FREETP_ARACH_STAGING "${CMAKE_BINARY_DIR}/_staging_freetp_arach")

add_custom_command(TARGET freetp_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_PLUGIN_DEPLOY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:freetp_plugin>"
        "${_PLUGIN_DEPLOY_DIR}/$<TARGET_FILE_NAME:freetp_plugin>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FREETP_PLUGIN_DIR}/plugin.json"
        "${_PLUGIN_DEPLOY_DIR}/plugin.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FREETP_CATALOG}"
        "${_PLUGIN_DEPLOY_DIR}/games-arachnel.json"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${_FREETP_ARACH_STAGING}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_FREETP_ARACH_STAGING}/freetp"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_PLUGIN_DEPLOY_DIR}/$<TARGET_FILE_NAME:freetp_plugin>"
        "${_FREETP_ARACH_STAGING}/freetp/$<TARGET_FILE_NAME:freetp_plugin>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_PLUGIN_DEPLOY_DIR}/plugin.json"
        "${_FREETP_ARACH_STAGING}/freetp/plugin.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_PLUGIN_DEPLOY_DIR}/games-arachnel.json"
        "${_FREETP_ARACH_STAGING}/freetp/games-arachnel.json"
    COMMENT "Stage freetp plugin bundle"
)

if(WIN32)
    add_custom_command(TARGET freetp_plugin POST_BUILD
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
            "if (Test-Path -LiteralPath '${_FREETP_ARACH}') { Remove-Item -LiteralPath '${_FREETP_ARACH}' -Force }; Compress-Archive -Path '${_FREETP_ARACH_STAGING}/freetp' -DestinationPath '${_FREETP_ARACH}' -Force"
        COMMENT "Package dist/freetp.arach"
    )
else()
    add_custom_command(TARGET freetp_plugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E chdir "${_FREETP_ARACH_STAGING}"
            ${CMAKE_COMMAND} -E tar cf "${_FREETP_ARACH}" --format=zip freetp
        COMMENT "Package dist/freetp.arach"
    )
endif()

add_custom_target(freetp_arach ALL DEPENDS freetp_plugin)
