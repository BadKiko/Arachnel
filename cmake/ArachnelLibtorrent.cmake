include_guard(GLOBAL)

function(arachnel_find_boost_include_dir root out_var)
    if(EXISTS "${root}/boost/version.hpp")
        set(${out_var} "${root}" PARENT_SCOPE)
        return()
    endif()
    if(EXISTS "${root}/boost_1_84_0/boost/version.hpp")
        set(${out_var} "${root}/boost_1_84_0" PARENT_SCOPE)
        return()
    endif()
    file(GLOB_RECURSE _version_hpp "${root}/libs/*/include/boost/version.hpp")
    if(_version_hpp)
        list(GET _version_hpp 0 _first)
        get_filename_component(_boost_dir "${_first}" DIRECTORY)
        get_filename_component(_include_dir "${_boost_dir}" DIRECTORY)
        set(${out_var} "${_include_dir}" PARENT_SCOPE)
        return()
    endif()
    message(FATAL_ERROR "boost/version.hpp not found under ${root}")
endfunction()

function(arachnel_ensure_boost out_var)
    if(DEFINED ARACHNEL_BOOST_INCLUDE_DIR AND EXISTS "${ARACHNEL_BOOST_INCLUDE_DIR}/boost/version.hpp")
        set(${out_var} "${ARACHNEL_BOOST_INCLUDE_DIR}" PARENT_SCOPE)
        return()
    endif()

    find_package(Boost 1.69 QUIET)
    if(Boost_FOUND)
        if(EXISTS "${Boost_INCLUDE_DIRS}/boost/version.hpp")
            set(${out_var} "${Boost_INCLUDE_DIRS}" PARENT_SCOPE)
            return()
        endif()
        if(EXISTS "${Boost_INCLUDE_DIR}/boost/version.hpp")
            set(${out_var} "${Boost_INCLUDE_DIR}" PARENT_SCOPE)
            return()
        endif()
    endif()

    include(FetchContent)
    message(STATUS "Boost: downloading headers (first run may take a few minutes)")

    FetchContent_Declare(
        arachnel_boost
        URL https://archives.boost.io/release/1.84.0/source/boost_1_84_0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(arachnel_boost)

    arachnel_find_boost_include_dir("${arachnel_boost_SOURCE_DIR}" _include_dir)
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

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
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
endfunction()
