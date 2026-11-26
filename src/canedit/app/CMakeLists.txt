# Copyright © 2026 CCP ehf.

# CanEdit: graphical editor for animation networks (.cane documents, exported as .can files)
find_package(tinyfiledialogs CONFIG REQUIRED)

if(WIN32)
    enable_language(RC)
endif()

set(CANEDIT_SOURCES
    appContext.h
    application.cpp
    application.h
    fileDialogs.cpp
    fileDialogs.h
    graphCanvas.cpp
    graphCanvas.h
    main.cpp
    nodeInfo.cpp
    nodeInfo.h
    panels.cpp
    panels.h
    preview.cpp
    preview.h
    stateMachineCanvas.cpp
    stateMachineCanvas.h
    stdafx.h
)
if(WIN32)
    list(APPEND CANEDIT_SOURCES CanEdit.rc)
endif()

add_executable(CanEdit ${CANEDIT_SOURCES})
set_build_flavor(CanEdit)
set_property(TARGET CanEdit PROPERTY COMPILE_WARNING_AS_ERROR ON)
ccp_enable_vs_analizers(CanEdit)
target_link_libraries(CanEdit PRIVATE CanEditCore CarbonMeshRender tinyfiledialogs::tinyfiledialogs)
target_precompile_headers(CanEdit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stdafx.h)
target_compile_options(CanEdit PRIVATE $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-fms-extensions>)

if(APPLE)
    set_target_properties(CanEdit PROPERTIES BUILD_WITH_INSTALL_RPATH FALSE LINK_FLAGS "-Wl,-rpath,./")
endif()

if(INSTALL_TO_MONOLITH)
    install(
        TARGETS CanEdit
        CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
        RUNTIME DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
    )
    install(
        FILES $<TARGET_RUNTIME_DLLS:CanEdit>
        DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
    )
else()
    install(
        TARGETS CanEdit
        CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
        RUNTIME DESTINATION bin
    )
endif()

if(WIN32)
    add_custom_command(TARGET CanEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:CanEdit> $<TARGET_FILE_DIR:CanEdit>
        COMMAND_EXPAND_LISTS
    )
endif()
