cmake_minimum_required(VERSION 2.8)
project (crow_examples)

if (MSVC)
	add_executable(example_vs example_vs.cpp)
	target_link_libraries(example_vs ${Boost_LIBRARIES})
	target_link_libraries(example_vs ${CMAKE_THREAD_LIBS_INIT})
else ()
	add_executable(helloworld helloworld.cpp)
	target_link_libraries(helloworld ${Boost_LIBRARIES})
	target_link_libraries(helloworld ${CMAKE_THREAD_LIBS_INIT})

if (OPENSSL_FOUND)
	add_executable(example_ssl ssl/example_ssl.cpp)
	target_link_libraries(example_ssl ${Boost_LIBRARIES})
	target_link_libraries(example_ssl ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES})
endif()

add_executable(example_websocket websocket/example_ws.cpp)
target_link_libraries(example_websocket ${Boost_LIBRARIES})
target_link_libraries(example_websocket ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES})
add_custom_command(OUTPUT ws.html
    COMMAND ${CMAKE_COMMAND} -E
    copy ${PROJECT_SOURCE_DIR}/websocket/templates/ws.html ${CMAKE_CURRENT_BINARY_DIR}/templates/ws.html
    DEPENDS ${PROJECT_SOURCE_DIR}/websocket/templates/ws.html
)
add_custom_target(example_ws_copy ALL DEPENDS ws.html)

add_executable(example example.cpp)
#target_link_libraries(example crow)
target_link_libraries(example ${Boost_LIBRARIES})
target_link_libraries(example ${CMAKE_THREAD_LIBS_INIT})

if (Tcmalloc_FOUND)
	target_link_libraries(example ${Tcmalloc_LIBRARIES})
endif(Tcmalloc_FOUND)

add_executable(example_with_all example_with_all.cpp)
add_dependencies(example_with_all amalgamation)
#target_link_libraries(example crow)
target_link_libraries(example_with_all ${Boost_LIBRARIES})
target_link_libraries(example_with_all ${CMAKE_THREAD_LIBS_INIT})

add_custom_command(OUTPUT example_test.py
	COMMAND ${CMAKE_COMMAND} -E
	copy ${PROJECT_SOURCE_DIR}/example_test.py ${CMAKE_CURRENT_BINARY_DIR}/example_test.py
	DEPENDS ${PROJECT_SOURCE_DIR}/example_test.py 
)
add_custom_target(example_copy ALL DEPENDS example_test.py)

add_executable(example_chat example_chat.cpp)
target_link_libraries(example_chat ${Boost_LIBRARIES})
target_link_libraries(example_chat ${CMAKE_THREAD_LIBS_INIT})
add_custom_command(OUTPUT example_chat.html
	COMMAND ${CMAKE_COMMAND} -E
	copy ${PROJECT_SOURCE_DIR}/example_chat.html ${CMAKE_CURRENT_BINARY_DIR}/example_chat.html
	DEPENDS ${PROJECT_SOURCE_DIR}/example_chat.html 
)
add_custom_target(example_chat_copy ALL DEPENDS example_chat.html)

#SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g -pg" )
#SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -g -pg" )
endif()
