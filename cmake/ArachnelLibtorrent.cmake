include_guard(GLOBAL)

function(arachnel_boost_headers_ok root)
    if(EXISTS "${root}/boost/config/user.hpp")
        set(ARACHNEL_BOOST_HEADERS_OK TRUE PARENT_SCOPE)
        return()
    endif()
    if(EXISTS "${root}/boost_1_84_0/boost/config/user.hpp")
        set(ARACHNEL_BOOST_HEADERS_OK TRUE PARENT_SCOPE)
        return()
    endif()
    file(GLOB_RECURSE _user_hpp "${root}/libs/*/include/boost/config/user.hpp")
    if(_user_hpp)
        set(ARACHNEL_BOOST_HEADERS_OK TRUE PARENT_SCOPE)
        return()
    endif()
    set(ARACHNEL_BOOST_HEADERS_OK FALSE PARENT_SCOPE)
endfunction()

function(arachnel_find_boost_include_dir root out_var)
    if(EXISTS "${root}/boost/config/user.hpp")
        set(${out_var} "${root}" PARENT_SCOPE)
        return()
    endif()
    if(EXISTS "${root}/boost_1_84_0/boost/config/user.hpp")
        set(${out_var} "${root}/boost_1_84_0" PARENT_SCOPE)
        return()
    endif()
    file(GLOB_RECURSE _user_hpp "${root}/libs/*/include/boost/config/user.hpp")
    if(_user_hpp)
        list(GET _user_hpp 0 _first)
        get_filename_component(_config_dir "${_first}" DIRECTORY)
        get_filename_component(_boost_dir "${_config_dir}" DIRECTORY)
        get_filename_component(_include_dir "${_boost_dir}" DIRECTORY)
        set(${out_var} "${_include_dir}" PARENT_SCOPE)
        return()
    endif()
    message(FATAL_ERROR "boost/config/user.hpp not found under ${root}")
endfunction()

function(arachnel_repair_incomplete_boost_fetch)
    set(_boost_src "${CMAKE_BINARY_DIR}/_deps/arachnel_boost-src")
    if(NOT EXISTS "${_boost_src}")
        return()
    endif()
    arachnel_boost_headers_ok("${_boost_src}")
    if(ARACHNEL_BOOST_HEADERS_OK)
        return()
    endif()
    message(STATUS "Boost: repairing incomplete headers download ...")
    file(REMOVE_RECURSE "${_boost_src}")
    file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/_deps/arachnel_boost-subbuild")
    unset(ARACHNEL_BOOST_INCLUDE_DIR CACHE)
endfunction()

function(arachnel_ensure_boost out_var)
    if(DEFINED ARACHNEL_BOOST_INCLUDE_DIR)
        arachnel_boost_headers_ok("${ARACHNEL_BOOST_INCLUDE_DIR}")
        if(ARACHNEL_BOOST_HEADERS_OK)
            set(${out_var} "${ARACHNEL_BOOST_INCLUDE_DIR}" PARENT_SCOPE)
            return()
        endif()
        unset(ARACHNEL_BOOST_INCLUDE_DIR CACHE)
    endif()

    find_package(Boost 1.69 QUIET)
    if(Boost_FOUND)
        foreach(_candidate IN ITEMS "${Boost_INCLUDE_DIRS}" "${Boost_INCLUDE_DIR}")
            if(_candidate)
                arachnel_boost_headers_ok("${_candidate}")
                if(ARACHNEL_BOOST_HEADERS_OK)
                    set(${out_var} "${_candidate}" PARENT_SCOPE)
                    return()
                endif()
            endif()
        endforeach()
    endif()

    include(FetchContent)
    arachnel_repair_incomplete_boost_fetch()
    message(STATUS "Boost: downloading headers (first run may take a few minutes)")

    FetchContent_Declare(
        arachnel_boost
        URL https://archives.boost.io/release/1.84.0/source/boost_1_84_0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(arachnel_boost)

    arachnel_find_boost_include_dir("${arachnel_boost_SOURCE_DIR}" _include_dir)
    set(ARACHNEL_BOOST_INCLUDE_DIR "${_include_dir}" CACHE PATH "Boost headers for bundled libtorrent")
    set(${out_var} "${_include_dir}" PARENT_SCOPE)
endfunction()

function(arachnel_export_boost_for_libtorrent include_dir)
    set(BOOST_ROOT "${include_dir}" CACHE PATH "Boost root for bundled libtorrent" FORCE)
    set(Boost_INCLUDE_DIR "${include_dir}" CACHE PATH "Boost include dir for bundled libtorrent" FORCE)
    set(Boost_NO_SYSTEM_PATHS ON CACHE BOOL "" FORCE)
    message(STATUS "Boost include dir for libtorrent: ${include_dir}")
endfunction()

function(arachnel_setup_libtorrent target)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBTORRENT QUIET IMPORTED_TARGET libtorrent-rasterbar)
    endif()

    if(LIBTORRENT_FOUND)
        message(STATUS "libtorrent: system package (pkg-config)")
        target_link_libraries(${target} PRIVATE PkgConfig::LIBTORRENT)
        return()
    endif()

    message(STATUS "libtorrent: building from source (no system package found)")

    arachnel_ensure_boost(_boost_include_dir)
    arachnel_export_boost_for_libtorrent("${_boost_include_dir}")

    set(_LT_SRC "${CMAKE_SOURCE_DIR}/thirdparty/libtorrent")
    set(_LT_BIN "${CMAKE_BINARY_DIR}/thirdparty/libtorrent")

    if(ARACHNEL_LIBTORRENT_SHARED)
        set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
        message(STATUS "libtorrent: shared library (faster dev links)")
    else()
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        message(STATUS "libtorrent: static library")
    endif()

    set(build_tests OFF CACHE BOOL "" FORCE)
    set(build_examples OFF CACHE BOOL "" FORCE)
    set(build_tools OFF CACHE BOOL "" FORCE)
    set(python-bindings OFF CACHE BOOL "" FORCE)
    set(encryption OFF CACHE BOOL "" FORCE)
    set(webtorrent OFF CACHE BOOL "" FORCE)

    include(FetchContent)

    if(EXISTS "${_LT_SRC}/CMakeLists.txt")
        message(STATUS "libtorrent: thirdparty/libtorrent")
        if(NOT TARGET torrent-rasterbar)
            add_subdirectory("${_LT_SRC}" "${_LT_BIN}" EXCLUDE_FROM_ALL)
        endif()
    else()
        message(STATUS "libtorrent: fetching v2.0.11")
        FetchContent_Declare(
            libtorrent
            GIT_REPOSITORY https://github.com/arvidn/libtorrent.git
            GIT_TAG v2.0.11
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(libtorrent)
    endif()

    if(NOT TARGET torrent-rasterbar)
        message(FATAL_ERROR "libtorrent target torrent-rasterbar was not created")
    endif()

    if(MINGW)
        target_compile_options(torrent-rasterbar PRIVATE "-Wa,-mbig-obj")
    endif()

    target_link_libraries(${target} PRIVATE torrent-rasterbar)

    if(WIN32 AND ARACHNEL_LIBTORRENT_SHARED)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:torrent-rasterbar>"
                "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Copy libtorrent DLL next to ${target}"
        )
    endif()
endfunction()
