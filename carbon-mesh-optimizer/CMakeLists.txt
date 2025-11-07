project(CarbonMeshOptimizer VERSION 0.1.0)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(carbon-math CONFIG REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(mikktspace CONFIG REQUIRED)


include_directories(
    src
)

file(GLOB_RECURSE SOURCES
    src/*.*
)

add_executable(CarbonMeshOptimizer ${SOURCES})

target_link_libraries(CarbonMeshOptimizer PUBLIC CarbonMesh CcpMath meshoptimizer::meshoptimizer mikktspace::mikktspace)
target_precompile_headers(CarbonMeshOptimizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/stdafx.h)

target_compile_options(CarbonMeshOptimizer PRIVATE $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-fms-extensions>)
target_compile_features(CarbonMeshOptimizer PRIVATE cxx_std_17)
set_target_properties(CarbonMeshOptimizer PROPERTIES
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
        TARGETS CarbonMeshOptimizer
        EXPORT CarbonMeshOptimizerTarget
        CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
        LIBRARY DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        ARCHIVE DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        RUNTIME DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
)

install(
    FILES $<TARGET_RUNTIME_DLLS:CarbonMeshOptimizer>
    DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
)

add_custom_command(TARGET CarbonMeshOptimizer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:CarbonMeshOptimizer> $<TARGET_RUNTIME_DLLS:CarbonMeshOptimizer>
  COMMAND_EXPAND_LISTS
)

