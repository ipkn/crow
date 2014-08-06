cmake_minimum_required(VERSION 2.8)
project (crow_examples)
 
add_executable(example example.cpp)
target_link_libraries(example crow)
target_link_libraries(example ${Boost_LIBRARIES} )
set_target_properties(example PROPERTIES COMPILE_FLAGS "-Wall -std=c++1y")

add_executable(example_chat example_chat.cpp)
target_link_libraries(example_chat crow)
target_link_libraries(example_chat ${Boost_LIBRARIES} )
set_target_properties(example_chat PROPERTIES COMPILE_FLAGS "-Wall -std=c++1y")
message(${CMAKE_CURRENT_BINARY_DIR})
message(${PROJECT_SOURCE_DIR})
add_custom_command(OUTPUT example_chat.html
        #TARGET example_chat 
        COMMAND ${CMAKE_COMMAND} -E
        copy ${PROJECT_SOURCE_DIR}/example_chat.html ${CMAKE_CURRENT_BINARY_DIR}/example_chat.html
        DEPENDS ${PROJECT_SOURCE_DIR}/example_chat.html 
        )
add_custom_target(example_chat_copy ALL DEPENDS example_chat.html)
