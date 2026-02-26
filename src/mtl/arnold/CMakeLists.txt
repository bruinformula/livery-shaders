file(GLOB AUTOLIB_ARNOLD_CPP_PLUGIN_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(autolib_arnold_plugin SHARED ${AUTOLIB_ARNOLD_CPP_PLUGIN_FILES})

target_link_libraries(autolib_arnold_plugin PRIVATE Arnold::Arnold)

set(AUTOLIB_ARNOLD_PLUGIN_DIR "${CMAKE_BINARY_DIR}/autolib_arnold_plugin")

set_target_properties(autolib_arnold_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${AUTOLIB_ARNOLD_PLUGIN_DIR}
)