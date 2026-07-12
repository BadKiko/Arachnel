# Patch QmlMaterial: on WIN32 it forces STATIC, which breaks MinGW plugin linking.
# Also drop OPTIMIZED shaders — spirv-opt comes from Vulkan SDK, often missing on MinGW setups.
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
file(WRITE "${SRC}/CMakeLists.txt" "${_content}")
