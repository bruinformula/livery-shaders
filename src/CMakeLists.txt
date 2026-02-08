
add_library(shared STATIC "${CMAKE_CURRENT_SOURCE_DIR}/OSLCompiler.cpp")

target_include_directories(shared BEFORE PRIVATE ${OpenImageIO_INCLUDES})
target_include_directories(shared BEFORE PRIVATE ${OSL_INCLUDES})
target_include_directories(shared BEFORE PRIVATE ${MaterialX_INCLUDES})

target_link_libraries(shared PRIVATE
    OSL::oslquery
    OSL::oslcomp
)
target_link_libraries(shared PRIVATE
    MaterialXGenOsl
)

# MaterialXExporter

add_executable(MaterialXExporter "${CMAKE_CURRENT_SOURCE_DIR}/MaterialXExporter.cpp")

target_include_directories(MaterialXExporter BEFORE PRIVATE ${OpenImageIO_INCLUDES})
target_include_directories(MaterialXExporter BEFORE PRIVATE ${OSL_INCLUDES})
target_include_directories(MaterialXExporter BEFORE PRIVATE ${MaterialX_INCLUDES})

target_link_libraries(MaterialXExporter PRIVATE
    OSL::oslquery
    OSL::oslcomp
)

target_link_libraries(MaterialXExporter PRIVATE
    shared
)

set_target_properties(MaterialXExporter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# MaterialXLibraryBuilder

add_executable(MaterialXLibraryBuilder "${CMAKE_CURRENT_SOURCE_DIR}/MaterialXLibraryBuilder.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/GLSLStubs.cpp")

target_include_directories(MaterialXLibraryBuilder BEFORE PRIVATE ${OpenImageIO_INCLUDES})
target_include_directories(MaterialXLibraryBuilder BEFORE PRIVATE ${OSL_INCLUDES})
target_include_directories(MaterialXLibraryBuilder BEFORE PRIVATE ${MaterialX_INCLUDES})

target_link_libraries(MaterialXLibraryBuilder PRIVATE
    OSL::oslquery
    OSL::oslcomp
)

target_link_libraries(MaterialXLibraryBuilder PRIVATE
    shared
)

set_target_properties(MaterialXLibraryBuilder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

