# Copyright © 2026 CCP ehf.

set(CANEDIT_TEST_SOURCES
    clipboardtest.cpp
    documenttest.cpp
    loadertest.cpp
    main.cpp
    sessiontest.cpp
    settingstest.cpp
    statemachinetest.cpp
    testutils.h
)

find_package(GTest CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(canedittest ${CANEDIT_TEST_SOURCES})
set_build_flavor(canedittest)
set_property(TARGET canedittest PROPERTY COMPILE_WARNING_AS_ERROR ON)
target_link_libraries(canedittest PRIVATE CanEditCore GTest::gtest nlohmann_json::nlohmann_json)
# Reuses the in-memory test assets of the CarbonMesh tests
target_include_directories(canedittest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../cmf/test)

if(WIN32)
    add_custom_command(
            TARGET canedittest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:canedittest>
            $<TARGET_FILE_DIR:canedittest>
            COMMAND_EXPAND_LISTS
            VERBATIM
    )
endif()
