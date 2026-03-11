find_package(OSL REQUIRED)
find_package(OpenImageIO REQUIRED)
find_package(MaterialX REQUIRED)


add_library(mtlShared STATIC "${CMAKE_CURRENT_SOURCE_DIR}/OSLCompiler.cpp")

target_include_directories(mtlShared BEFORE PRIVATE ${OpenImageIO_INCLUDES})
target_include_directories(mtlShared BEFORE PRIVATE ${OSL_INCLUDES})
target_include_directories(mtlShared BEFORE PRIVATE ${MaterialX_INCLUDES})

target_link_libraries(mtlShared PRIVATE
    OSL::oslquery
    OSL::oslcomp
)
target_link_libraries(mtlShared PRIVATE
    MaterialXGenOsl
)

# MaterialXOSOExporter
add_executable(MaterialXOSOExporter "${CMAKE_CURRENT_SOURCE_DIR}/MaterialXOSOExporter.cpp")

target_include_directories(MaterialXOSOExporter BEFORE PRIVATE ${OSL_INCLUDES})
target_include_directories(MaterialXOSOExporter BEFORE PRIVATE ${MaterialX_INCLUDES})

target_link_libraries(MaterialXOSOExporter PRIVATE
    OSL::oslquery
    OSL::oslcomp
)

target_link_libraries(MaterialXOSOExporter PRIVATE
    mtlShared
)

set_target_properties(MaterialXOSOExporter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# MaterialXLibraryBuilder
add_executable(MaterialXLibraryBuilder "${CMAKE_CURRENT_SOURCE_DIR}/MaterialXLibraryBuilder.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/GLSLStubs.cpp")

target_include_directories(MaterialXLibraryBuilder BEFORE PRIVATE ${OSL_INCLUDES})
target_include_directories(MaterialXLibraryBuilder BEFORE PRIVATE ${MaterialX_INCLUDES})

target_link_libraries(MaterialXLibraryBuilder PRIVATE
    OSL::oslquery
    OSL::oslcomp
)

target_link_libraries(MaterialXLibraryBuilder PRIVATE
    mtlShared
)

set_target_properties(MaterialXLibraryBuilder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# MaterialXQuery
add_executable(MaterialXNodeQuery "${CMAKE_CURRENT_SOURCE_DIR}/MaterialXNodeQuery.cpp")

target_include_directories(MaterialXNodeQuery BEFORE PRIVATE ${OSL_INCLUDES})
target_include_directories(MaterialXNodeQuery BEFORE PRIVATE ${MaterialX_INCLUDES})

target_link_libraries(MaterialXNodeQuery PRIVATE
    OSL::oslquery
    OSL::oslcomp
)

target_link_libraries(MaterialXNodeQuery PRIVATE
    mtlShared
)

set_target_properties(MaterialXNodeQuery PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Arnold Plugin
if (${BUILD_AUTOLIB_MTL_ARNOLD_BSDFS})
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/arnold")
endif()