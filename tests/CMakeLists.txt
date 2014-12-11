cmake_minimum_required(VERSION 2.8)
project (crow_test)
 

set(TEST_SRCS
unittest.cpp
)

add_executable(unittest ${TEST_SRCS})
#target_link_libraries(unittest crow)
target_link_libraries(unittest ${Boost_LIBRARIES})
target_link_libraries(unittest ${CMAKE_THREAD_LIBS_INIT})

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
# using Clang
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
# using GCC
set_target_properties(unittest PROPERTIES COMPILE_FLAGS "--coverage -fprofile-arcs -ftest-coverage")
target_link_libraries(unittest gcov)
endif()

add_subdirectory(template)
#CXXFLAGS="-g -O0 -Wall -W -Wshadow -Wunused-variable \
#Wunused-parameter -Wunused-function -Wunused -Wno-system-headers \
#-Wno-deprecated -Woverloaded-virtual -Wwrite-strings -fprofile-arcs -ftest-coverage"
#CFLAGS="-g -O0 -Wall -W -fprofile-arcs -ftest-coverage"
#LDFLAGS="-fprofile-arcs -ftest-coverage"
