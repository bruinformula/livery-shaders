find_package(OpenImageIO REQUIRED)
find_package(msdfgen REQUIRED)


# svgtx
add_executable(svgtx "${CMAKE_CURRENT_SOURCE_DIR}/svgtx.cpp")

target_include_directories(svgtx BEFORE PRIVATE ${OpenImageIO_INCLUDES})

target_link_libraries(svgtx PRIVATE
    msdfgen::msdfgen
    OpenImageIO::OpenImageIO
)

install(TARGETS svgtx
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)