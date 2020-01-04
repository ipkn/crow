cmake_minimum_required(VERSION 2.8)
project (crow_all)

if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(Tcmalloc)
find_package(Threads)
find_package(OpenSSL)

if(OPENSSL_FOUND)
	include_directories(${OPENSSL_INCLUDE_DIR})
endif()

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  message("Found ccache ${CCACHE_FOUND}")
  message("Using ccache to speed up compilation")
  set(ENV{CCACHE_CPP2} "yes")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

if (NOT CMAKE_BUILD_TYPE)
	message(STATUS "No build type selected, default to Release")
	set(CMAKE_BUILD_TYPE "Release")
endif()

if (MSVC)
	set(Boost_USE_STATIC_LIBS "On")
	find_package( Boost 1.52 COMPONENTS system thread regex REQUIRED )
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++1y -pedantic -Wextra")
	find_package( Boost 1.52 COMPONENTS system thread REQUIRED )
endif()

include_directories( ${Boost_INCLUDE_DIR} )

set(PROJECT_INCLUDE_DIR
    ${PROJECT_SOURCE_DIR}/include
)

include_directories("${PROJECT_INCLUDE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}")

#add_subdirectory(src)
add_subdirectory(examples)

if (MSVC)
else()
	add_subdirectory(tests)

	enable_testing()
	add_test(NAME crow_test COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tests/unittest)
	add_test(NAME template_test COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tests/template/test.py WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests/template)

	file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/amalgamate)

	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/amalgamate/crow_all.h
		COMMAND python ${PROJECT_SOURCE_DIR}/amalgamate/merge_all.py ${PROJECT_SOURCE_DIR}/include
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/amalgamate/crow_all.h ${PROJECT_SOURCE_DIR}/amalgamate
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/amalgamate
		DEPENDS ${PROJECT_SOURCE_DIR}/include/*.h ${PROJECT_SOURCE_DIR}/include/crow/*.h
	)

	add_custom_target(amalgamation ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/amalgamate/crow_all.h)
endif()
