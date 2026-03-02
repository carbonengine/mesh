set(FBX_IMPORT_SOURCES
    animation.h
    animation.cpp
    lodsimplygon.cpp
    main.cpp
    mesh.h
    mesh.cpp
    options.h
    options.cpp
    skeleton.h
    skeleton.cpp
    transform.h
    transform.cpp
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

add_executable(fbximport ${FBX_IMPORT_SOURCES} ${UFBX_SOURCES})
target_link_libraries(fbximport PRIVATE CarbonMesh CLI11::CLI11 unofficial::hash-library)
if(USE_SIMPLYGON)
    target_compile_definitions(fbximport PRIVATE USE_SIMPLYGON)
    target_link_libraries(fbximport PRIVATE Simplygon)
endif()
target_include_directories(fbximport PRIVATE "../../vendor/github.com/ufbx/ufbx")
source_group(UFBX FILES ${UFBX_SOURCES})
set_property(TARGET fbximport PROPERTY COMPILE_WARNING_AS_ERROR ON)
ccp_enable_vs_analizers(fbximport)

if (INSTALL_TO_MONOLITH)
    install(
            TARGETS fbximport
            EXPORT fbximportTarget
            CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
            LIBRARY DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
            ARCHIVE DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
            RUNTIME DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
    )

    install(
        FILES $<TARGET_RUNTIME_DLLS:fbximport>
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
        TARGET fbximport POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:fbximport> $<TARGET_FILE_DIR:fbximport>
        COMMAND_EXPAND_LISTS
    )
    if(USE_SIMPLYGON)
        get_target_property(SIMPLYGON_DLL Simplygon SIMPLYGON_DLL_LOCATION)
        add_custom_command(
            TARGET fbximport POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SIMPLYGON_DLL} $<TARGET_FILE_DIR:fbximport>
            COMMAND_EXPAND_LISTS
        )
    endif()
endif()

