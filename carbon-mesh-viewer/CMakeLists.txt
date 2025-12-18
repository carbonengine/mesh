project(CarbonMeshViewer VERSION 0.1.0)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(imgui REQUIRED)
find_package(carbon-core CONFIG REQUIRED)
find_package(carbon-math CONFIG REQUIRED)


set( SHADER_BINARY_TO_SHADER_CACHE ${CMAKE_CURRENT_SOURCE_DIR}/bin/shaderCacheCreator.py)  
set( GENERATED_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/GeneratedShaders)  

if( WIN32 )
    set( SHADER_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/bin/glslc.exe)    
else()
    set( SHADER_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/bin/glslc)       
endif()

file( GLOB SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.*
)

include_directories(
    src
    src/vulkan
    src/mesh
    src/input
)

file(GLOB_RECURSE SOURCES
    src/*.*
    shaders/*.*
)

add_executable(CarbonMeshViewer ${SOURCES} ${GENERATED_SHADER_DIR})
set(COMPILED_SHADERS "")

foreach( SHADER_PATH ${SHADERS} )
    file( RELATIVE_PATH SHADER_RELATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/shaders ${SHADER_PATH} )

    get_filename_component( SHADER_NAME ${SHADER_RELATIVE_PATH} NAME )

    if( NOT EXISTS ${GENERATED_SHADER_DIR} )
        file( MAKE_DIRECTORY ${GENERATED_SHADER_DIR} )
    endif()

    add_custom_command(
        OUTPUT "${GENERATED_SHADER_DIR}/${SHADER_NAME}.spv.h" 
        DEPENDS ${SHADER_PATH} 
        COMMAND ${SHADER_COMPILER} "${SHADER_PATH}" -mfmt=num -o "${GENERATED_SHADER_DIR}/${SHADER_NAME}.spv.h" 
        COMMENT "Compiling shader: ${SHADER_PATH} to ${GENERATED_SHADER_DIR}/${SHADER_NAME}.spv.h"
    )
    list( APPEND COMPILED_SHADERS "${GENERATED_SHADER_DIR}/${SHADER_NAME}.spv.h" )
endforeach()

add_custom_command( 
    OUTPUT "${GENERATED_SHADER_DIR}/generatedShaderCache.h"
    DEPENDS ${SHADERS} 
    COMMAND python ${SHADER_BINARY_TO_SHADER_CACHE} "${GENERATED_SHADER_DIR}" "${GENERATED_SHADER_DIR}/generatedShaderCache.h"
    COMMENT "Generating shader cache header ${GENERATED_SHADER_DIR}/generatedShaderCache.h"
)

list( APPEND COMPILED_SHADERS "${GENERATED_SHADER_DIR}/generatedShaderCache.h" )

add_custom_target(CompileShaders DEPENDS ${COMPILED_SHADERS})
add_dependencies(CarbonMeshViewer CompileShaders)


target_link_libraries(CarbonMeshViewer PUBLIC CarbonMesh CcpMath CcpCore glfw Vulkan::Vulkan imgui::imgui)
target_precompile_headers(CarbonMeshViewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/stdafx.h)
target_include_directories(CarbonMeshViewer PRIVATE ${GENERATED_SHADER_DIR})

target_compile_options(CarbonMeshViewer PRIVATE $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-fms-extensions>)
target_compile_features(CarbonMeshViewer PRIVATE cxx_std_17)
set_target_properties(CarbonMeshViewer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

if(CMAKE_GENERATOR STREQUAL "Xcode" OR CMAKE_GENERATOR MATCHES "Visual Studio.*")
    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        PREFIX "Sources"
        FILES ${SOURCES}
    )
endif()

install(
        TARGETS CarbonMeshViewer
        EXPORT CarbonMeshViewerTarget
        CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
        LIBRARY DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        ARCHIVE DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        RUNTIME DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
)

install(
    FILES $<TARGET_RUNTIME_DLLS:CarbonMeshViewer>
    DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
)

add_custom_command(TARGET CarbonMeshViewer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:CarbonMeshViewer> $<TARGET_RUNTIME_DLLS:CarbonMeshViewer>
  COMMAND_EXPAND_LISTS
)

