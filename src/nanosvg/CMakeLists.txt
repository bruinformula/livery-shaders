add_library(nanosvg INTERFACE)

target_include_directories(nanosvg INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(nanosvg INTERFACE ${MATH_LIBRARY})
endif()