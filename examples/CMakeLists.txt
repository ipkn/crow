cmake_minimum_required(VERSION 2.8)
project (crow_examples)
 
add_executable(example example.cpp)
target_link_libraries(example crow)
target_link_libraries(example ${Boost_LIBRARIES} )
add_custom_command(OUTPUT example_test.py
        COMMAND ${CMAKE_COMMAND} -E
        copy ${PROJECT_SOURCE_DIR}/example_test.py ${CMAKE_CURRENT_BINARY_DIR}/example_test.py
        DEPENDS ${PROJECT_SOURCE_DIR}/example_test.py 
        )
add_custom_target(example_copy ALL DEPENDS example_test.py)

add_executable(example_chat example_chat.cpp)
target_link_libraries(example_chat crow)
target_link_libraries(example_chat ${Boost_LIBRARIES} )
add_custom_command(OUTPUT example_chat.html
        COMMAND ${CMAKE_COMMAND} -E
        copy ${PROJECT_SOURCE_DIR}/example_chat.html ${CMAKE_CURRENT_BINARY_DIR}/example_chat.html
        DEPENDS ${PROJECT_SOURCE_DIR}/example_chat.html 
        )
add_custom_target(example_chat_copy ALL DEPENDS example_chat.html)
