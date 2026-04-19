find_package(OSL REQUIRED)
find_package(OpenImageIO REQUIRED)
find_package(MaterialX REQUIRED)

# AutolibMtl
add_library(AutolibMtl STATIC "${CMAKE_CURRENT_SOURCE_DIR}/OSLCompileOptions.cpp")

target_include_directories(AutolibMtl BEFORE PRIVATE ${OpenImageIO_INCLUDES})
target_include_directories(AutolibMtl BEFORE PRIVATE ${OSL_INCLUDES})
target_include_directories(AutolibMtl BEFORE PRIVATE ${MaterialX_INCLUDES})

target_link_libraries(AutolibMtl PRIVATE
    OSL::oslquery
    OSL::oslcomp
)

install(TARGETS AutolibMtl
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# OSLCompiler
add_executable(OSLCompiler "${CMAKE_CURRENT_SOURCE_DIR}/OSLCompiler.cpp")

target_include_directories(OSLCompiler BEFORE PRIVATE ${OSL_INCLUDES})

target_link_libraries(OSLCompiler PRIVATE
    OSL::oslquery
    OSL::oslcomp
)

target_link_libraries(OSLCompiler PRIVATE
    AutolibMtl
)

install(TARGETS OSLCompiler
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Arnold Plugin
if (${BUILD_AUTOLIB_MTL_ARNOLD})
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/arnold")
endif()