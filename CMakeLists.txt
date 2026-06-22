# Copyright © 2025 CCP ehf.

cmake_minimum_required(VERSION 3.30)

set(WITH_SIMPLYGON_DEFAULT_FLAG OFF)

option(WITH_SIMPLYGON "Use Simplygon for LOD generation" ${WITH_SIMPLYGON_DEFAULT_FLAG})

set(BUILD_TOOLS_DEFAULT_FLAG OFF)
option(BUILD_TOOLS "Build tools such as the mesh processor and viewer" ${BUILD_TOOLS_DEFAULT_FLAG})
if(BUILD_TOOLS)
	message("Building tools such as the mesh processor and viewer")
	list(APPEND VCPKG_MANIFEST_FEATURES "tools")
	if(WITH_SIMPLYGON)
		message("Using Simplygon for LOD generation")
		list(APPEND VCPKG_MANIFEST_FEATURES "simplygon")
	endif()
endif()

project(CarbonMesh VERSION 1.0.0)

find_package(carbon-math CONFIG REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(mikktspace CONFIG REQUIRED)

include(cmake/CcpBuildConfigurations.cmake)
include(cmake/CcpTargetConfigurations.cmake)
include(GenerateExportHeader)

set(PUBLIC_HEADER_FILES
    include/cmf/animation.h
    include/cmf/bounds.h
    include/cmf/bufferstreams.h
    include/cmf/bufferutils.h
    include/cmf/cmf.h
    include/cmf/compression.h
    include/cmf/converters.h
    include/cmf/declutils.h
    include/cmf/memallocator.h
    include/cmf/span.h
    include/cmf/tangents.h
    include/cmf/transforms.h
    include/cmf/utils.h
    include/cmf/uvdensity.h
    include/cmf/v1.h
    include/cmf/writer.h
)

set(CARBON_MESH_SOURCE_FILES
    ${PUBLIC_HEADER_FILES}
    src/cmf/animation.cpp
    src/cmf/bounds.cpp
    src/cmf/bufferutils.cpp
    src/cmf/compression.cpp
    src/cmf/declutils.cpp
    src/cmf/memallocator.cpp
    src/cmf/tangents.cpp
    src/cmf/utils.cpp
    src/cmf/uvdensity.cpp
    src/cmf/writer.cpp
)


function(ccp_enable_vs_analizers target)
    set_target_properties(${target} PROPERTIES
        VS_GLOBAL_RunCodeAnalysis false
        VS_GLOBAL_EnableMicrosoftCodeAnalysis false
        VS_GLOBAL_EnableClangTidyCodeAnalysis true
    )
endfunction()


add_library(CarbonMesh SHARED
    ${CARBON_MESH_SOURCE_FILES}
)
generate_export_header(CarbonMesh BASE_NAME CARBON_MESH)
set_build_flavor(CarbonMesh)

if (CMAKE_GENERATOR MATCHES "Visual Studio")
    target_sources(CarbonMesh PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/cmf.natvis>
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/cmf/cmf.natvis>
    )
    target_link_options(CarbonMesh PUBLIC 
        $<BUILD_INTERFACE:/NATVIS:${CMAKE_CURRENT_LIST_DIR}/src/cmf.natvis>
        $<INSTALL_INTERFACE:/NATVIS:$<INSTALL_PREFIX>/include/cmf/cmf.natvis>
    )
endif()

get_target_property(_SOURCES CarbonMesh SOURCES)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        PREFIX "Sources"
        FILES ${_SOURCES}
)

set_target_properties(CarbonMesh PROPERTIES OUTPUT_NAME "CarbonMesh")
target_link_libraries(CarbonMesh PUBLIC 
	CcpMath 
	$<BUILD_INTERFACE:meshoptimizer::meshoptimizer>
	$<BUILD_INTERFACE:mikktspace::mikktspace>)
target_include_directories(CarbonMesh PUBLIC         
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
set_property(TARGET CarbonMesh PROPERTY COMPILE_WARNING_AS_ERROR ON)
ccp_enable_vs_analizers(CarbonMesh)

# Provide an install target if this is the top level project only
if(CMAKE_PROJECT_NAME STREQUAL CarbonMesh)
    # Determine if/when to generate/build documentation. The rules are as follows:
    # - If running locally - Build documentation is OFF by default
    # - If running on a build agent (TeamCity) - Build documentation is ON by default
    # - If -DBUILD_DOCUMENTATION=ON/OFF flag is explicitly set, use it.
    set(BUILD_DOCUMENTATION_DEFAULT_FLAG OFF)
    if(DEFINED ENV{TEAMCITY_VERSION})
        set(BUILD_DOCUMENTATION_DEFAULT_FLAG ON)
    endif()
    message(STATUS "Document generation settings: default=${BUILD_DOCUMENTATION_DEFAULT_FLAG}, option BUILD_DOCUMENTATION=${BUILD_DOCUMENTATION}")
    option(BUILD_DOCUMENTATION "Build Documentation" ${BUILD_DOCUMENTATION_DEFAULT_FLAG})

    if(BUILD_DOCUMENTATION)
        # Set up sphinx build for Python autodoc comments.
        set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/doc/source)
        set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR}/doc/build)

        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        include(cmake/CcpDocsGenerator.cmake)

        create_carbon_docs_sphinx_target(
                PYTHON_EXE ${Python3_EXECUTABLE}
                VENV_NAME docs_generation_venv
                PYTHONPATH_ENV $ENV{PYTHONPATH}
                SPHINX_SOURCE ${SPHINX_SOURCE}
                SPHINX_BUILD ${SPHINX_BUILD}
                DOXYGEN_SRC_FILES ${CARBON_MESH_SOURCE_FILES}
                SPHINX_TARGET_NAME Sphinx
                DOXYGEN_TARGET_NAME Doxygen
                INSTALL_DESTINATION documentation
        )

        add_dependencies(Sphinx CarbonMesh)
    endif()

    option(INSTALL_TO_MONOLITH "Install this package to a perforce branch vendor folder - provided for backwards compatibility" OFF)
    if (INSTALL_TO_MONOLITH)
        # Install rule to ensure that our runtime and linker files are in the expected, platform-specific folders
        install(
                TARGETS CarbonMesh
                EXPORT CarbonMeshTarget
                CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
                LIBRARY DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
                ARCHIVE DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
                RUNTIME DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        )

         # Install rule for available public headers
        install(FILES ${PUBLIC_HEADER_FILES} DESTINATION include/cmf)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/carbon_mesh_export.h DESTINATION include/cmf)

        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CarbonMeshConfig.cmake DESTINATION .)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/cmf.natvis DESTINATION .)
    else ()
        # Install rule to ensure that our runtime and linker files are in the vcpkg structure
        install(
                TARGETS CarbonMesh
                EXPORT CarbonMeshTarget
                CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
                LIBRARY DESTINATION lib
                ARCHIVE DESTINATION lib
                RUNTIME DESTINATION bin
        )

         # Install rule for available public headers
        install(FILES ${PUBLIC_HEADER_FILES} DESTINATION include/cmf)

        install(EXPORT CarbonMeshTarget
                DESTINATION share/CarbonMesh
                FILE CarbonMeshTargets.cmake
        )

        include(CMakePackageConfigHelpers)

        configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CarbonMeshConfig.cmake.in 
                                      ${CMAKE_CURRENT_BINARY_DIR}/CarbonMeshConfig.cmake
                                      INSTALL_DESTINATION share/CarbonMesh)

        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CarbonMeshConfig.cmake DESTINATION share/CarbonMesh)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/cmf.natvis DESTINATION include/cmf)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/carbon_mesh_export.h DESTINATION include/cmf)
    endif()
endif()

if(BUILD_TOOLS)
	add_subdirectory( src/viewer )
	add_subdirectory( src/processor )
endif()

add_subdirectory (src/cmf/test) 
enable_testing()
add_test(NAME cmftest COMMAND cmftest)