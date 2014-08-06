cmake_minimum_required(VERSION 2.8)
project (crow_all)
 
find_package( Boost 1.40 COMPONENTS date_time filesystem system thread REQUIRED )
#find_package( Tcmalloc )
include_directories( ${Boost_INCLUDE_DIR} )

#set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

#set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
#set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(PROJECT_INCLUDE_DIR 
${PROJECT_SOURCE_DIR}/include
${PROJECT_SOURCE_DIR}/http-parser
)

include_directories("${PROJECT_INCLUDE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}")
 
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(examples)
