cmake_minimum_required(VERSION 2.8)
project (template_test)
 

set(PROJECT_INCLUDE_DIR 
${PROJECT_SOURCE_DIR}/include
)

set(TEST_SRCS
mustachetest.cpp
)

add_executable(mustachetest ${TEST_SRCS})
#target_link_libraries(unittest crow)
#target_link_libraries(unittest ${Boost_LIBRARIES} )
set_target_properties(mustachetest PROPERTIES COMPILE_FLAGS "-Wall -std=c++1y")

#message(${PROJECT_SOURCE_DIR})
#message(${CMAKE_CURRENT_BINARY_DIR})
file(COPY DIRECTORY . DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
    FILES_MATCHING 
    PATTERN "*.json")

add_custom_command(OUTPUT test.py
        COMMAND ${CMAKE_COMMAND} -E
        copy ${PROJECT_SOURCE_DIR}/test.py ${CMAKE_CURRENT_BINARY_DIR}/test.py
        DEPENDS ${PROJECT_SOURCE_DIR}/test.py
        )
add_custom_target(template_test_copy ALL DEPENDS test.py)
