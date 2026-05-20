set(PROCESSOR_SOURCES
    cmffile.h
    cmffile.cpp
    commands.cpp
    commands.h
    dumpjson.cpp
    extractlod.cpp
    generatelods.cpp
    hash.cpp
    main.cpp
    metadata.cpp
    validate.cpp
)

set(FBXIMPORT_SOURCES
    fbximport/animation.h
    fbximport/animation.cpp
    fbximport/fbximport.cpp
    fbximport/lodsimplygon.cpp
    fbximport/mesh.h
    fbximport/mesh.cpp
    fbximport/options.h
    fbximport/options.cpp
    fbximport/skeleton.h
    fbximport/skeleton.cpp
    fbximport/transform.h
    fbximport/transform.cpp
)

set(UFBX_SOURCES
    ../../vendor/github.com/ufbx/ufbx/ufbx.h
    ../../vendor/github.com/ufbx/ufbx/ufbx.c
)


find_package(CLI11 CONFIG REQUIRED)
find_package(unofficial-hash-library CONFIG REQUIRED)

if(USE_SIMPLYGON)
    find_package(Simplygon CONFIG REQUIRED)
endif()

add_executable(cmfprocessor ${PROCESSOR_SOURCES} ${UFBX_SOURCES} ${FBXIMPORT_SOURCES})
target_link_libraries(cmfprocessor PRIVATE CarbonMesh CLI11::CLI11 unofficial::hash-library)
if(USE_SIMPLYGON)
    target_compile_definitions(cmfprocessor PRIVATE USE_SIMPLYGON)
    target_link_libraries(cmfprocessor PRIVATE Simplygon)
endif()
target_include_directories(cmfprocessor PRIVATE "../../vendor/github.com/ufbx/ufbx")
source_group(UFBX FILES ${UFBX_SOURCES})
source_group(fbximport FILES ${FBXIMPORT_SOURCES})
set_property(TARGET cmfprocessor PROPERTY COMPILE_WARNING_AS_ERROR ON)
ccp_enable_vs_analizers(cmfprocessor)

if (INSTALL_TO_MONOLITH)
    install(
            TARGETS cmfprocessor
            EXPORT cmfprocessorTarget
            CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
            LIBRARY DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
            ARCHIVE DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
            RUNTIME DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
    )

    install(
        FILES $<TARGET_RUNTIME_DLLS:cmfprocessor>
        DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
    )
    if (WIN32)
        if(USE_SIMPLYGON)
            get_target_property(SIMPLYGON_DLL Simplygon SIMPLYGON_DLL_LOCATION)
            install(
                FILES ${SIMPLYGON_DLL}
                DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
            )
        endif()
    endif()

endif()

if (WIN32)
    add_custom_command(
        TARGET cmfprocessor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:cmfprocessor> $<TARGET_FILE_DIR:cmfprocessor>
        COMMAND_EXPAND_LISTS
    )
    if(USE_SIMPLYGON)
        get_target_property(SIMPLYGON_DLL Simplygon SIMPLYGON_DLL_LOCATION)
        add_custom_command(
            TARGET cmfprocessor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SIMPLYGON_DLL} $<TARGET_FILE_DIR:cmfprocessor>
            COMMAND_EXPAND_LISTS
        )
    endif()
endif()

