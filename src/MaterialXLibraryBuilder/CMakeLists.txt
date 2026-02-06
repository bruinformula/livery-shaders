add_executable(main "${CMAKE_CURRENT_SOURCE_DIR}/MaterialXLibraryBuilder.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/GLSLStubs.cpp")

target_include_directories(main BEFORE PRIVATE ${OpenImageIO_INCLUDES})
target_include_directories(main BEFORE PRIVATE ${OSL_INCLUDES})
target_include_directories(main BEFORE PRIVATE ${MaterialX_INCLUDES})

target_link_libraries(main PRIVATE
    OSL::oslquery
    OSL::oslcomp
)


target_link_libraries(main PRIVATE
    MaterialXGenOsl
    MaterialXGenGlsl
)

set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
