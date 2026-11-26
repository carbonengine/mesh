# Copyright © 2026 CCP ehf.

# Private static library with the Vulkan renderer shared by the CarbonMesh tools (viewer, animation network editor)

find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(imgui REQUIRED)
find_package(carbon-math CONFIG REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(VCPKG_INSTALLED_PATH "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

set(SHADER_BINARY_TO_SHADER_CACHE ${CMAKE_CURRENT_SOURCE_DIR}/scripts/shaderCacheCreator.py)
set(SOURCE_SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/)
set(GENERATED_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/GeneratedShaders)
set(GLSLC_EXECUTABLE "${VCPKG_INSTALLED_PATH}/tools/shaderc/glslc.exe")
if(APPLE)
    set(GLSLC_EXECUTABLE "${VCPKG_INSTALLED_PATH}/tools/shaderc/glslc")
endif()

message(STATUS "Using glslc executable at: ${GLSLC_EXECUTABLE}")

file(GLOB SHADERS ${SOURCE_SHADER_DIR}/*.*)

file(GLOB_RECURSE RENDER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(CarbonMeshRender STATIC ${RENDER_SOURCES} ${SHADERS})
set_build_flavor(CarbonMeshRender)
set_property(TARGET CarbonMeshRender PROPERTY COMPILE_WARNING_AS_ERROR ON)
ccp_enable_vs_analizers(CarbonMeshRender)
target_compile_features(CarbonMeshRender PUBLIC cxx_std_17)

set(COMPILED_SHADERS "")

# Compile shaders to SPIR-V and generate header files
foreach(SHADER_PATH ${SHADERS})
    file(RELATIVE_PATH SHADER_RELATIVE_PATH ${SOURCE_SHADER_DIR} ${SHADER_PATH})

    get_filename_component(SHADER_NAME ${SHADER_RELATIVE_PATH} NAME)
    get_filename_component(SHADER_EXTENSION ${SHADER_RELATIVE_PATH} EXT)

    if(NOT EXISTS ${GENERATED_SHADER_DIR})
        file(MAKE_DIRECTORY ${GENERATED_SHADER_DIR})
    endif()
    if("${SHADER_EXTENSION}" STREQUAL ".comp" OR "${SHADER_EXTENSION}" STREQUAL ".vert" OR "${SHADER_EXTENSION}" STREQUAL ".frag")
        add_custom_command(
            OUTPUT "${GENERATED_SHADER_DIR}/${SHADER_NAME}.spv.h"
            DEPENDS ${SHADER_PATH}
            COMMAND ${GLSLC_EXECUTABLE} "${SHADER_PATH}" -mfmt=num -o "${GENERATED_SHADER_DIR}/${SHADER_NAME}.spv.h"
            COMMENT "Compiling shader: ${SHADER_PATH} to ${GENERATED_SHADER_DIR}/${SHADER_NAME}.spv.h"
        )
        list(APPEND COMPILED_SHADERS "${GENERATED_SHADER_DIR}/${SHADER_NAME}.spv.h")
    endif()
endforeach()

# shader cache generation
add_custom_command(
    OUTPUT "${GENERATED_SHADER_DIR}/generatedShaderCache.h"
    DEPENDS ${SHADERS} ${COMPILED_SHADERS} ${SHADER_BINARY_TO_SHADER_CACHE}
    COMMAND ${Python3_EXECUTABLE} ${SHADER_BINARY_TO_SHADER_CACHE} "${SOURCE_SHADER_DIR}" "${GENERATED_SHADER_DIR}" "${GENERATED_SHADER_DIR}/generatedShaderCache.h"
    COMMENT "Generating shader cache header ${GENERATED_SHADER_DIR}/generatedShaderCache.h"
)

list(APPEND COMPILED_SHADERS "${GENERATED_SHADER_DIR}/generatedShaderCache.h")

add_custom_target(CompileShaders DEPENDS ${COMPILED_SHADERS})
add_dependencies(CarbonMeshRender CompileShaders)

target_link_libraries(CarbonMeshRender
    PUBLIC
        CarbonMesh
        CcpMath
        glfw
        Vulkan::Vulkan
        imgui::imgui
)
target_include_directories(CarbonMeshRender
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${GENERATED_SHADER_DIR}
)
target_precompile_headers(CarbonMeshRender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/renderpch.h)
target_compile_options(CarbonMeshRender PRIVATE $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-fms-extensions>)

if(APPLE)
    # Portability subset definition needed for MoltenVK
    # Beta extensions are needed for the portability subset in MoltenVK
    target_compile_definitions(CarbonMeshRender PUBLIC VKB_ENABLE_PORTABILITY VK_ENABLE_BETA_EXTENSIONS APPLE)
endif()

if(CMAKE_GENERATOR STREQUAL "Xcode" OR CMAKE_GENERATOR MATCHES "Visual Studio.*")
    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        PREFIX "Sources"
        FILES ${RENDER_SOURCES} ${SHADERS}
    )
endif()

set_directory_properties(
    PROPERTIES
    ADDITIONAL_MAKE_CLEAN_FILES "${GENERATED_SHADER_DIR}"
)
