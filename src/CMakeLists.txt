find_package(OpenImageIO REQUIRED)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/nanosvg")

# svgtx
add_executable(svgtx "${CMAKE_CURRENT_SOURCE_DIR}/svgtx.cpp")

target_include_directories(svgtx BEFORE PRIVATE ${OpenImageIO_INCLUDES})

target_link_libraries(svgtx PRIVATE
    nanosvg
    OpenImageIO::OpenImageIO
)

install(TARGETS svgtx
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Arnold Plugin
if (${BUILD_AUTOLIB_MTL_ARNOLD})
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/arnold")
endif()