add_executable(main main.cpp)

target_include_directories(main BEFORE PRIVATE ${OpenImageIO_INCLUDES})
target_include_directories(main BEFORE PRIVATE ${OSL_INCLUDES})
target_include_directories(main BEFORE PRIVATE ${MaterialX_INCLUDES})

target_link_libraries(main PRIVATE
    OSL::oslexec
    OSL::oslquery
    OSL::oslcomp
)

set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
