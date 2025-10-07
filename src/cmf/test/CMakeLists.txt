set(CMF_TEST_SOURCES
    main.cpp
)
    
find_package(GTest REQUIRED)

add_executable(cmftest ${CMF_TEST_SOURCES})
target_link_libraries(cmftest PRIVATE CarbonMesh GTest::GTest GTest::Main)
