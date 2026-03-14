find_package(Arnold REQUIRED)

set(AUTOLIB_ARNOLD_PLUGIN_DIR "${CMAKE_BINARY_DIR}/autolib/arnold_plugin")

set(AUTOLIB_ARNOLD_PLUGIN_INSTALL_DIR
    "${CMAKE_INSTALL_PREFIX}//autolib/autolib_plugin"
    CACHE PATH "Arnold plugin install directory"
)

if(NOT DEFINED CACHE{AUTOLIB_ARNOLD_PLUGIN_INSTALL_DIR} 
   AND DEFINED ENV{AUTOLIB_ARNOLD_PLUGIN_INSTALL_DIR})
    set(AUTOLIB_ARNOLD_PLUGIN_INSTALL_DIR "$ENV{AUTOLIB_ARNOLD_PLUGIN_INSTALL_DIR}"
        CACHE PATH "Arnold plugin install directory" FORCE)
endif()

function(add_arnold_plugin PLUGIN_NAME)
    set(ARNOLD_PLUGIN_NAME "${PLUGIN_NAME}")
    set(ARNOLD_PLUGIN_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_NAME}")

    file(GLOB ARNOLD_CPP_SOURCES CONFIGURE_DEPENDS
        "${ARNOLD_PLUGIN_DIR}/*.cpp"
    )

    add_library(${ARNOLD_PLUGIN_NAME} SHARED ${ARNOLD_CPP_SOURCES})
    target_link_libraries(${ARNOLD_PLUGIN_NAME} PRIVATE Arnold::Arnold)

    set_target_properties(${ARNOLD_PLUGIN_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${AUTOLIB_ARNOLD_PLUGIN_DIR}
    )

    install(TARGETS ${ARNOLD_PLUGIN_NAME}
        LIBRARY DESTINATION "${AUTOLIB_ARNOLD_PLUGIN_INSTALL_DIR}"
    )
endfunction()

add_arnold_plugin(diffuse_ramp)
add_arnold_plugin(phong_ramp)
add_arnold_plugin(diffuse_toon)
add_arnold_plugin(glossy_toon)