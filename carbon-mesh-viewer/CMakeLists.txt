project(CarbonMeshViewer VERSION 0.1.0)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(imgui REQUIRED)
find_package(carbon-core CONFIG REQUIRED)
find_package(carbon-math CONFIG REQUIRED)

include_directories(
    src
    src/vulkan
    src/mesh
    src/ui
    src/input
)

file(GLOB_RECURSE SOURCES
    src/*.*
)

add_executable(CarbonMeshViewer ${SOURCES})

target_link_libraries(CarbonMeshViewer PUBLIC CarbonMesh CcpMath CcpCore glfw Vulkan::Vulkan imgui::imgui)
target_precompile_headers(CarbonMeshViewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/stdafx.h)

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

install(DIRECTORY shaders/ DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/shaders
        FILES_MATCHING
        PATTERN "*.spv"
)
#file( COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:CarbonMeshViewer> shaders )