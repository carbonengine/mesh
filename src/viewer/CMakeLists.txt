# Copyright © 2025 CCP ehf.

project(CarbonMeshViewer VERSION 0.1.0)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(WIN32)
    enable_language(RC)
endif()

find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(imgui REQUIRED)
find_package(carbon-math CONFIG REQUIRED)
find_package(tinyfiledialogs  CONFIG REQUIRED)

set(VCPKG_INSTALLED_PATH "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

set( SHADER_BINARY_TO_SHADER_CACHE ${CMAKE_CURRENT_SOURCE_DIR}/scripts/shaderCacheCreator.py)  
set( SOURCE_SHADER_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/)  
set( GENERATED_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/GeneratedShaders)  
set( GLSLC_EXECUTABLE "${VCPKG_INSTALLED_PATH}/tools/shaderc/glslc.exe")
set( ENV{VK_LAYER_PATH} "${VCPKG_INSTALLED_PATH}/bin")
set( ENV{VULKAN_SDK} "${VCPKG_INSTALLED_PATH}")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(APPLE)
    set( ENV{VK_LAYER_PATH} "${VCPKG_INSTALLED_PATH}/share/vulkan/explicit_layer.d")
    set( GLSLC_EXECUTABLE "${VCPKG_INSTALLED_PATH}/tools/shaderc/glslc")
    set( ENV{VK_DRIVER_FILES} "${_VCPKG_INSTALLED_DIR}/../../vendor/github.com/moltenVK/MoltenVK/dylib/macOS/MoltenVK_icd.json")
endif()

set( CMAKE_INSTALL_RPATH "$ORIGIN/" )

message(STATUS "Using glslc executable at: ${GLSLC_EXECUTABLE}")
message(STATUS "VK layer path: $ENV{VK_LAYER_PATH}")
message(STATUS "VK VK_DRIVER_FILES path: $ENV{VK_DRIVER_FILES}")


file( GLOB SHADERS
    ${SOURCE_SHADER_DIR}/*.*
)

include_directories(
	.
    $ENV{VULKAN_SDK}/include
)

link_directories($ENV{VULKAN_SDK}/lib)

file(GLOB_RECURSE SOURCES
    *.*
)

set(RESOURCE_FILES "")
if(WIN32)
    list(APPEND RESOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/CarbonMeshViewerResource.h
        ${CMAKE_CURRENT_SOURCE_DIR}/CarbonMeshViewer.rc
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/carbon_trans-16.ico
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/carbon_trans-24.ico
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/carbon_trans-32.ico
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/carbon_trans-48.ico
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/carbon_trans-64.ico
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/carbon_trans-96.ico
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/carbon_trans-128.ico
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/carbon_trans-256.ico
    )
endif()

add_executable(CarbonMeshViewer ${SOURCES} ${RESOURCE_FILES} ${GENERATED_SHADER_DIR})
set_build_flavor(CarbonMeshViewer)

set_property(TARGET CarbonMeshViewer PROPERTY COMPILE_WARNING_AS_ERROR ON)
ccp_enable_vs_analizers(CarbonMeshViewer)

set(COMPILED_SHADERS "")

foreach( SHADER_PATH ${SHADERS} )
    file( RELATIVE_PATH SHADER_RELATIVE_PATH ${SOURCE_SHADER_DIR} ${SHADER_PATH} )

    get_filename_component( SHADER_NAME ${SHADER_RELATIVE_PATH} NAME )
	get_filename_component( SHADER_EXTENSION ${SHADER_RELATIVE_PATH} EXT )

    if( NOT EXISTS ${GENERATED_SHADER_DIR} )
        file( MAKE_DIRECTORY ${GENERATED_SHADER_DIR} )
    endif()
	if( "${SHADER_EXTENSION}" STREQUAL ".comp" OR "${SHADER_EXTENSION}" STREQUAL ".vert" OR "${SHADER_EXTENSION}" STREQUAL ".frag" )
		add_custom_command(
			OUTPUT "${GENERATED_SHADER_DIR}/${SHADER_NAME}.spv.h" 
			DEPENDS ${SHADER_PATH} 
			COMMAND ${GLSLC_EXECUTABLE} "${SHADER_PATH}" -mfmt=num -o "${GENERATED_SHADER_DIR}/${SHADER_NAME}.spv.h" 
			COMMENT "Compiling shader: ${SHADER_PATH} to ${GENERATED_SHADER_DIR}/${SHADER_NAME}.spv.h"
		)
		list( APPEND COMPILED_SHADERS "${GENERATED_SHADER_DIR}/${SHADER_NAME}.spv.h" )
    endif()
endforeach()

add_custom_command( 
    OUTPUT "${GENERATED_SHADER_DIR}/generatedShaderCache.h"
    DEPENDS ${SHADERS} ${COMPILED_SHADERS} ${SHADER_BINARY_TO_SHADER_CACHE}
    COMMAND ${Python3_EXECUTABLE} ${SHADER_BINARY_TO_SHADER_CACHE} "${SOURCE_SHADER_DIR}" "${GENERATED_SHADER_DIR}" "${GENERATED_SHADER_DIR}/generatedShaderCache.h"
    COMMENT "Generating shader cache header ${GENERATED_SHADER_DIR}/generatedShaderCache.h"
)

list( APPEND COMPILED_SHADERS "${GENERATED_SHADER_DIR}/generatedShaderCache.h" )

message(STATUS "generated files: ${COMPILED_SHADERS}")

add_custom_target(CompileShaders DEPENDS ${COMPILED_SHADERS})
add_dependencies(CarbonMeshViewer CompileShaders)

target_link_libraries(CarbonMeshViewer PUBLIC CarbonMesh CcpMath glfw Vulkan::Vulkan imgui::imgui tinyfiledialogs::tinyfiledialogs)
target_precompile_headers(CarbonMeshViewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stdafx.h)
target_include_directories(CarbonMeshViewer PRIVATE ${GENERATED_SHADER_DIR})

target_compile_options(CarbonMeshViewer PRIVATE $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-fms-extensions>)
target_compile_features(CarbonMeshViewer PRIVATE cxx_std_17)

if(APPLE)

set_target_properties(CarbonMeshViewer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    BUILD_WITH_INSTALL_RPATH FALSE
    LINK_FLAGS "-Wl,-rpath,./"
)
else()
set_target_properties(CarbonMeshViewer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)
endif()

if(CMAKE_GENERATOR STREQUAL "Xcode" OR CMAKE_GENERATOR MATCHES "Visual Studio.*")
    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        PREFIX "Sources"
        FILES ${SOURCES} ${RESOURCE_FILES}
    )
endif()

IF (APPLE)
    # Automatically set Xcode scheme environment variables for Vulkan when building on macOS with Xcode
    if(APPLE AND CMAKE_GENERATOR STREQUAL "Xcode")
        set(xcode_environment_variables "")
        list( APPEND xcode_environment_variables "VK_DRIVER_FILES=$ENV{VK_DRIVER_FILES}" )
        list( APPEND xcode_environment_variables "VK_LAYER_PATH=$ENV{VK_LAYER_PATH}" )
        set_target_properties(CarbonMeshViewer PROPERTIES
            XCODE_SCHEME_ENVIRONMENT "${xcode_environment_variables}"
        )
        message(STATUS "Set Xcode scheme environment variables to ${xcode_environment_variables}")
    endif()
    
    # Portability subset definition needed for MoltenVK
    add_compile_definitions(VKB_ENABLE_PORTABILITY)
    # Beta extensions are needed for the portability subset in MoltenVK
    add_compile_definitions(VK_ENABLE_BETA_EXTENSIONS) 
    add_compile_definitions("APPLE")
ENDIF()

if (INSTALL_TO_MONOLITH)
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
else()
	install(
			TARGETS CarbonMeshViewer
			CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
			RUNTIME DESTINATION bin
	)
endif()

set_directory_properties(
    PROPERTIES
    ADDITIONAL_MAKE_CLEAN_FILES "${GENERATED_SHADER_DIR}"
)


add_custom_command(TARGET CarbonMeshViewer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:CarbonMeshViewer> $<TARGET_RUNTIME_DLLS:CarbonMeshViewer>
  COMMAND_EXPAND_LISTS
)

