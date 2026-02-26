find_path(ARNOLD_INCLUDE_DIR
    NAMES ai.h
    PATHS
        $ENV{ARNOLD_LOCATION}/include
        /usr/local/include
)

find_library(ARNOLD_LIBRARY
    NAMES ai libai
    PATHS
        $ENV{ARNOLD_LOCATION}/bin
        $ENV{ARNOLD_LOCATION}/lib
        /usr/local/bin
        /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Arnold
    REQUIRED_VARS ARNOLD_INCLUDE_DIR ARNOLD_LIBRARY
)

if(Arnold_FOUND)
    add_library(Arnold::Arnold UNKNOWN IMPORTED)
    set_target_properties(Arnold::Arnold PROPERTIES
        IMPORTED_LOCATION ${ARNOLD_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${ARNOLD_INCLUDE_DIR}
    )
endif()