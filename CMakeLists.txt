cmake_minimum_required(VERSION 2.8)
project (crow_all)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
find_package(Tcmalloc)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++1y")

find_package( Boost 1.40 COMPONENTS date_time filesystem system thread REQUIRED )
include_directories( ${Boost_INCLUDE_DIR} )

set(PROJECT_INCLUDE_DIR 
${PROJECT_SOURCE_DIR}/include
${PROJECT_SOURCE_DIR}/http-parser
)

include_directories("${PROJECT_INCLUDE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}")
 
#add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(examples)

enable_testing()
add_test(NAME crow_test COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tests/unittest)
add_test(NAME template_test COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tests/template/test.py WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests/template)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/amalgamate)
add_custom_target(amalgamation ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/amalgamate/crow_all.h)
file(COPY DIRECTORY ${PROJECT_SOURCE_DIR}/include DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
    FILES_MATCHING 
    PATTERN "*.h")

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/amalgamate/crow_all.h 
    COMMAND python ${PROJECT_SOURCE_DIR}/amalgamate/merge_all.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/amalgamate
    )

