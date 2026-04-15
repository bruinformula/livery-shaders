#set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "include-what-you-use")
#add_compile_options(-ftime-trace)

if(BUILD_AUTOLIB_MTL)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/mtl")
endif()

if(BUILD_AUTOLIB_SIM)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/sim")
endif()
