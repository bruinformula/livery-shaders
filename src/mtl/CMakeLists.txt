find_package(OSL REQUIRED)
find_package(OpenImageIO REQUIRED)
find_package(MaterialX REQUIRED)


add_library(AutolibMtl STATIC "${CMAKE_CURRENT_SOURCE_DIR}/OSLCompiler.cpp")

target_include_directories(AutolibMtl BEFORE PRIVATE ${OpenImageIO_INCLUDES})
target_include_directories(AutolibMtl BEFORE PRIVATE ${OSL_INCLUDES})
target_include_directories(AutolibMtl BEFORE PRIVATE ${MaterialX_INCLUDES})

target_link_libraries(AutolibMtl PRIVATE
    OSL::oslquery
    OSL::oslcomp
)
target_link_libraries(AutolibMtl PRIVATE
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
    AutolibMtl
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
    AutolibMtl
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
    AutolibMtl
)

# Arnold Plugin
if (${BUILD_AUTOLIB_MTL_ARNOLD})
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/arnold")
endif()