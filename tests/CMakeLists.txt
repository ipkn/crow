cmake_minimum_required(VERSION 2.8)
project (crow_test)
 

set(PROJECT_INCLUDE_DIR 
${PROJECT_SOURCE_DIR}/include
${PROJECT_SOURCE_DIR}/http-parser
)

set(TEST_SRCS
unittest.cpp
)

add_executable(test ${TEST_SRCS})
target_link_libraries(test crow)
target_link_libraries( test ${Boost_LIBRARIES} )
set_target_properties(test PROPERTIES COMPILE_FLAGS "-Wall -std=c++1y")
