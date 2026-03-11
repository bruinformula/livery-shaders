find_package(Arnold REQUIRED)

set(AUTOLIB_ARNOLD_PLUGIN_DIR "${CMAKE_BINARY_DIR}/autolib/arnold_plugin")

function(add_arnold_plugin PLUGIN_NAME)
    set(ARNOLD_PLUGIN_NAME  "autolib_${PLUGIN_NAME}")
    set(ARNOLD_PLUGIN_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_NAME}")

    file(GLOB AUTOLIB_ARNOLD_CPP_PLUGIN_FILES CONFIGURE_DEPENDS
        "${ARNOLD_PLUGIN_DIR}/*.cpp"
    )
    add_library(${ARNOLD_PLUGIN_NAME} SHARED ${AUTOLIB_ARNOLD_CPP_PLUGIN_FILES})

    target_link_libraries(${ARNOLD_PLUGIN_NAME} PRIVATE Arnold::Arnold)

    set_target_properties(${ARNOLD_PLUGIN_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${AUTOLIB_ARNOLD_PLUGIN_DIR}
    )

endfunction()

add_arnold_plugin(diffuse_ramp)
add_arnold_plugin(phong_ramp)

add_arnold_plugin(diffuse_toon)
add_arnold_plugin(glossy_toon)