
find_package(pxr REQUIRED)
find_package(Chrono REQUIRED COMPONENTS vehicle)

add_executable(ChronoSimTest
    "${CMAKE_CURRENT_SOURCE_DIR}/ChronoSimTest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ChronoUsdChef.cpp"
)

target_include_directories(ChronoSimTest BEFORE PRIVATE ${pxr_INCLUDES})
target_include_directories(ChronoSimTest BEFORE PRIVATE ${Chrono_INCLUDES})

target_link_libraries(ChronoSimTest PRIVATE
    usd
    usdGeom
    usdPhysics
    usdShade
    sdf
    tf
)

target_link_libraries(ChronoSimTest PRIVATE ${CHRONO_TARGETS})
#target_link_libraries(ChronoSimTest PRIVATE MPI::MPI_CXX)
#target_link_libraries(ChronoSimTest PRIVATE OpenMP::OpenMP_CXX)

set_target_properties(ChronoSimTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)