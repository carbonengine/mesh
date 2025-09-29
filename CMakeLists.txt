cmake_minimum_required(VERSION 3.16)
project(CarbonMesh VERSION 1.0.0)

find_package(carbon-math CONFIG REQUIRED)

include(cmake/CcpGlobalSettings.cmake)
include(cmake/CcpTargetConfigurations.cmake)

set(PUBLIC_HEADER_FILES
    include/cmf/cmf.h
    include/cmf/memallocator.h
    include/cmf/v1.h
    include/cmf/utils.h
    include/cmf/writer.h
)

ccp_add_library(CarbonMesh 
    ${PUBLIC_HEADER_FILES}
    src/cmf/memallocator.cpp
    src/cmf/utils.cpp
    src/cmf/writer.cpp
)

get_target_property(_SOURCES CarbonMesh SOURCES)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        PREFIX "Sources"
        FILES ${_SOURCES}
)

set_target_properties(CarbonMesh PROPERTIES OUTPUT_NAME "CarbonMesh")
target_link_libraries(CarbonMesh PUBLIC CcpMath)
target_include_directories(CarbonMesh PUBLIC         
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

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
        # Find Doxygen, which is needed to parse C++ source files
        find_package(Doxygen)

        set(DOCUMENTATION_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/doc)

        # Evaluate config file for Doxygen to input project values
        set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in)
        set(DOXYFILE_OUT ${DOCUMENTATION_OUT_DIR}/Doxyfile)
        configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

        set(DOXYGEN_OUT_DIR ${DOCUMENTATION_OUT_DIR}/xml)
        set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUT_DIR}/index.xml)

        # Regenerate with source changes
        add_custom_command(OUTPUT ${DOXYGEN_INDEX_FILE}
                DEPENDS ${_SOURCES}
                COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
                MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
                COMMENT "Running Doxygen"
                VERBATIM)

        add_custom_target(Doxygen ALL DEPENDS ${DOXYGEN_INDEX_FILE})

        # Set up sphinx build for Python autodoc comments.
        set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/doc/source)
        set(SPHINX_BUILD ${DOCUMENTATION_OUT_DIR}/build)

        # Select the correct pythonInterpreter to use based on build system.
        if(WIN32)
            set(CCP_PYTHON_INTERPRETER "${BRANCH_ROOT_DIR}/eve/client/pythonInterpreter.bat")
        elseif(APPLE)
            set(CCP_PYTHON_INTERPRETER "${BRANCH_ROOT_DIR}/eve/client/pythonInterpreter.sh")
        endif()

        # Run Sphinx against configuration file and Doxygen output
        add_custom_target(Sphinx ALL
                COMMAND ${CMAKE_COMMAND}
                -E env
                PYTHONPATH="$<TARGET_FILE_DIR:CarbonMesh>/;${CMAKE_SOURCE_DIR}/python"
                BUILDFLAVOR=$<LOWER_CASE:$<CONFIG>>
                ${CCP_PYTHON_INTERPRETER}
                -m sphinx build -E -b html
                -Dbreathe_projects.doxygen=${DOXYGEN_OUT_DIR}
                -c ${SPHINX_SOURCE}
                ${SPHINX_SOURCE} ${SPHINX_BUILD}
                WORKING_DIRECTORY ${BRANCH_ROOT_DIR}
                DEPENDS ${DOXYGEN_INDEX_FILE}
                COMMENT "Generating documentation with Sphinx")

        # Install rule for documentation
        install(DIRECTORY ${SPHINX_BUILD} DESTINATION documentation)
    endif()

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
    foreach(header_file ${PUBLIC_HEADER_FILES})
        get_filename_component(relative_path ${header_file} DIRECTORY)
        string(REPLACE "include/" "" relative_path ${relative_path})
        install(FILES ${header_file} DESTINATION include/${relative_path})
    endforeach()

    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/CarbonMeshConfig.cmake DESTINATION .)
    

    # Install python package
    install(DIRECTORY python/ DESTINATION python
            FILES_MATCHING
            PATTERN "*.py"
            PATTERN "package.yaml"
    )


endif()