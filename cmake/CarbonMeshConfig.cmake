# Copyright © 2025 CCP ehf.

include(CMakeFindDependencyMacro)



if(APPLE)
    set(_CARBON_MESH_LIBRARY_PREFIX "lib")
    set(_IMPORT_LIBRARY_SUFFIX ".a")
    set(_SHARED_LIBRARY_SUFFIX ".dylib")
else()
    set(_CARBON_MESH_LIBRARY_PREFIX "")
    set(_IMPORT_LIBRARY_SUFFIX ${CMAKE_IMPORT_LIBRARY_SUFFIX})
    set(_SHARED_LIBRARY_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
endif()

if(NOT TARGET CarbonMesh)
    set(_IMPORT_PREFIX ${CMAKE_CURRENT_LIST_DIR})
    add_library(CarbonMesh SHARED IMPORTED)
    set_target_properties(CarbonMesh PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
        INTERFACE_LINK_LIBRARIES ""
        INTERFACE_COMPILE_DEFINITIONS ""
        IMPORTED_CONFIGURATIONS "Debug;TrinityDev;Internal;Release"
    )
    set_property(TARGET CarbonMesh APPEND PROPERTY IMPORTED_CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES})
    foreach(CCT ${CMAKE_CONFIGURATION_TYPES})
        string(TOLOWER ${CCT} _LCCT)
        string(TOUPPER ${CCT} _UCCT)
        set(_IMP_LOC_PROP IMPORTED_LOCATION_${_UCCT})
        set(_IMP_IMP_PROP IMPORTED_IMPLIB_${_UCCT})
        get_target_property(_TARGET_TYPE CarbonMesh TYPE)
        if(${_TARGET_TYPE} STREQUAL SHARED_LIBRARY)
            if(WIN32)
                set(_VENDOR_PATH ${CCP_VENDOR_BIN_PATH})
            else()
                set(_VENDOR_PATH ${CCP_VENDOR_LIB_PATH})
            endif()
            if(${_LCCT} STREQUAL "release")
                set(_IMP_IMP_VAL ${_IMPORT_PREFIX}/${CCP_VENDOR_LIB_PATH}/${_CARBON_MESH_LIBRARY_PREFIX}CarbonMesh${_IMPORT_LIBRARY_SUFFIX})
            else()
                set(_IMP_IMP_VAL ${_IMPORT_PREFIX}/${CCP_VENDOR_LIB_PATH}/${_CARBON_MESH_LIBRARY_PREFIX}CarbonMesh_${_LCCT}${_IMPORT_LIBRARY_SUFFIX})
            endif()
            set_target_properties(CarbonMesh PROPERTIES ${_IMP_IMP_PROP} ${_IMP_IMP_VAL})
            set(_IMP_LOC_SUFFIX ${_SHARED_LIBRARY_SUFFIX})
        else()
            set(_VENDOR_PATH ${CCP_VENDOR_LIB_PATH})
            set(_IMP_LOC_SUFFIX ${_IMPORT_LIBRARY_SUFFIX})
        endif()
        if(${_LCCT} STREQUAL "release")
            set(_IMP_LOC_VAL ${_IMPORT_PREFIX}/${_VENDOR_PATH}/${_CARBON_MESH_LIBRARY_PREFIX}CarbonMesh${_IMP_LOC_SUFFIX})
        else()
            set(_IMP_LOC_VAL ${_IMPORT_PREFIX}/${_VENDOR_PATH}/${_CARBON_MESH_LIBRARY_PREFIX}CarbonMesh_${_LCCT}${_IMP_LOC_SUFFIX})
        endif()
        if(NOT EXISTS ${_IMP_LOC_VAL})
            message(FATAL_ERROR "Missing CarbonMesh library ${_IMP_LOC_VAL}, is a build and perforce publish for the '${CCT}' configuration missing?")
        endif()
        message(DEBUG "Setting CarbonMesh property ${_IMP_LOC_PROP} to ${_IMP_LOC_VAL}")
        set_target_properties(CarbonMesh PROPERTIES ${_IMP_LOC_PROP} ${_IMP_LOC_VAL})
    endforeach()

    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        set_target_properties(CarbonMesh PROPERTIES 
            INTERFACE_LINK_OPTIONS /NATVIS:"$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/cmf.natvis>"
            INTERFACE_SOURCES "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/cmf.natvis>"
        )
    endif()

endif()
