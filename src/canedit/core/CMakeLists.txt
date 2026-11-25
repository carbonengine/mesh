# Copyright © 2026 CCP ehf.

# Editing logic of CanEdit that doesn't depend on the UI, so that it can be tested
find_package(nlohmann_json CONFIG REQUIRED)

set(CANEDIT_CORE_SOURCES
    assetLoader.cpp
    assetLoader.h
    autoLayout.cpp
    autoLayout.h
    clipboard.cpp
    clipboard.h
    document.cpp
    document.h
    history.cpp
    history.h
    networkJson.cpp
    networkJson.h
    pathResolver.cpp
    pathResolver.h
    session.cpp
    session.h
    settings.cpp
    settings.h
)

add_library(CanEditCore STATIC ${CANEDIT_CORE_SOURCES})
set_build_flavor(CanEditCore)
set_property(TARGET CanEditCore PROPERTY COMPILE_WARNING_AS_ERROR ON)
ccp_enable_vs_analizers(CanEditCore)
target_compile_features(CanEditCore PUBLIC cxx_std_17)
target_link_libraries(CanEditCore PUBLIC CarbonMesh PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(CanEditCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
