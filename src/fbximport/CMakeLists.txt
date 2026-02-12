set(FBX_IMPORT_SOURCES
    animation.h
    animation.cpp
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

add_executable(fbximport ${FBX_IMPORT_SOURCES} ${UFBX_SOURCES})
target_link_libraries(fbximport PRIVATE CarbonMesh CLI11::CLI11 unofficial::hash-library)
target_include_directories(fbximport PRIVATE "../../vendor/github.com/ufbx/ufbx")
source_group(UFBX FILES ${UFBX_SOURCES})

if (WIN32)
    add_custom_command(
        TARGET fbximport POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:fbximport> $<TARGET_FILE_DIR:fbximport>
        COMMAND_EXPAND_LISTS
    )
endif()
