cmake_minimum_required(VERSION 3.15)
project(crow_test)

set(TEST_SRCS
  unittest.cpp
)

add_executable(unittest ${TEST_SRCS})
target_link_libraries(unittest ${Boost_LIBRARIES})
target_link_libraries(unittest ${CMAKE_THREAD_LIBS_INIT})

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set_target_properties(unittest PROPERTIES COMPILE_FLAGS "--coverage -fprofile-arcs -ftest-coverage -Wall -Werror")
  target_link_libraries(unittest gcov)
endif()

add_subdirectory(template)
add_subdirectory(img)
