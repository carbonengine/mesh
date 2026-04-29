set(PROCESSOR_SOURCES
    cmffile.h
    cmffile.cpp
    commands.cpp
    commands.h
    dumpjson.cpp
    extractlod.cpp
    hash.cpp
    main.cpp
    metadata.cpp
    validate.cpp
)

find_package(CLI11 CONFIG REQUIRED)
find_package(unofficial-hash-library CONFIG REQUIRED)

add_executable(cmfprocessor ${PROCESSOR_SOURCES})
target_link_libraries(cmfprocessor PRIVATE CarbonMesh CLI11::CLI11 unofficial::hash-library)
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

endif()

if (WIN32)
    add_custom_command(
        TARGET cmfprocessor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:cmfprocessor> $<TARGET_FILE_DIR:cmfprocessor>
        COMMAND_EXPAND_LISTS
    )
endif()

