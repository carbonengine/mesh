# Copyright © 2025 CCP ehf.

set(CMF_TEST_SOURCES
    animation.cpp
    main.cpp
    transforms.cpp
    writer.cpp
)
    
find_package(GTest CONFIG REQUIRED)

add_executable(cmftest ${CMF_TEST_SOURCES})
target_link_libraries(cmftest PRIVATE CarbonMesh GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

if (WIN32)
    add_custom_command(
            TARGET cmftest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_RUNTIME_DLLS:cmftest>
            $<TARGET_FILE_DIR:cmftest>
            COMMAND_EXPAND_LISTS
            VERBATIM
    )
endif ()
