if(BUILD_AUTOLIB_GEOM) 
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/geom")
endif()

if(BUILD_AUTOLIB_MTL)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/mtl")
endif()

