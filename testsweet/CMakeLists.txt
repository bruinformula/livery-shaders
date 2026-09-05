find_package(Python3 REQUIRED)

set(AUTOLIB_TESTSWEET_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(AUTOLIB_BUILD_DIR      "${CMAKE_BINARY_DIR}")
get_filename_component(ARNOLD_LIB_DIR "${ARNOLD_LIBRARY}" DIRECTORY)


function(add_usd_render_test TEST_NAME)
    set(TEST_SRC_DIR  "${AUTOLIB_TESTSWEET_DIR}/${TEST_NAME}")
    set(TEST_WORK_DIR "${CMAKE_BINARY_DIR}/testsweet/${TEST_NAME}")

    file(MAKE_DIRECTORY "${TEST_WORK_DIR}")

    # Symlink ref/ into the working directory
    set(REF_LINK "${TEST_WORK_DIR}/ref")
    set(REF_SRC  "${TEST_SRC_DIR}/ref")
    if(EXISTS "${REF_SRC}" AND NOT EXISTS "${REF_LINK}")
        if(WIN32)
            file(COPY "${REF_SRC}" DESTINATION "${TEST_WORK_DIR}")
        else()
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E create_symlink "${REF_SRC}" "${REF_LINK}"
            )
        endif()
    endif()

    # Symlink the test source directory as data/ so scene.usda is reachable as data/<file> from the working dir.
    set(DATA_LINK "${TEST_WORK_DIR}/data")
    if(NOT EXISTS "${DATA_LINK}")
        if(WIN32)
            file(COPY "${TEST_SRC_DIR}/" DESTINATION "${TEST_WORK_DIR}/data")
        else()
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                        "${TEST_SRC_DIR}" "${DATA_LINK}"
            )
        endif()
    endif()

    add_test(
        NAME "${TEST_NAME}"
        COMMAND ${CMAKE_COMMAND} -E env
            "PXR_PLUGINPATH_NAME=$ENV{PXR_PLUGINPATH_NAME}"
            "RMAN_SHADERPATH=$ENV{RMAN_SHADERPATH}"
            "${Python3_EXECUTABLE}"
            "${AUTOLIB_TESTSWEET_DIR}/runtest.py"
            "${TEST_SRC_DIR}"
            "${AUTOLIB_BUILD_DIR}"
        WORKING_DIRECTORY "${TEST_WORK_DIR}"
    )
    set_tests_properties("${TEST_NAME}" PROPERTIES
        ENVIRONMENT
            "
            AUTOLIB_TESTSWEET_DIR=${AUTOLIB_TESTSWEET_DIR};\
            Usd_TESTSUITE_SRC=${TEST_SRC_DIR};\
            AUTOLIB_BUILD_DIR=${AUTOLIB_BUILD_DIR};\
            RMAN_SHADERPATH=$ENV{RMAN_SHADERPATH};\
            PXR_PLUGINPATH_NAME=$ENV{PXR_PLUGINPATH_NAME};
            "
    )
endfunction()

add_usd_render_test(render-curvature)