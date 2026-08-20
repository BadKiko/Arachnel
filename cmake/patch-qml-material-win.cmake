# Patch QmlMaterial: on WIN32 it forces STATIC, which breaks MinGW plugin linking.
# Drop OPTIMIZED shaders (spirv-opt / Vulkan SDK) and PkgConfig REQUIRED
# (QmlMaterial never calls pkg_check_modules; local Qt kits have no pkg-config).
cmake_minimum_required(VERSION 3.20)
if(NOT DEFINED SRC)
    message(FATAL_ERROR "SRC not set")
endif()
file(READ "${SRC}/CMakeLists.txt" _content)
string(REPLACE
    "    set(QML_MATERIAL_BUILD_TYPE STATIC)"
    "    set(QML_MATERIAL_BUILD_TYPE SHARED)"
    _content "${_content}")
string(REPLACE
    "set(SHADER_OPT BATCHABLE OPTIMIZED)"
    "set(SHADER_OPT BATCHABLE)"
    _content "${_content}")
if(WIN32)
    string(REGEX REPLACE "find_package\\(PkgConfig REQUIRED\\)[^\n]*\n" "" _content "${_content}")
endif()
file(WRITE "${SRC}/CMakeLists.txt" "${_content}")
