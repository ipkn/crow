cmake_minimum_required(VERSION 2.8)
project (crow)

set(CROW_SRCS 
#${PROJECT_SOURCE_DIR}/some.cpp
#${PROJECT_SOURCE_DIR}/someother.cpp
${PROJECT_SOURCE_DIR}/../http-parser/http_parser.c
)

set_source_files_properties(${PROJECT_SOURCE_DIR}/../http-parser/http_parser.c PROPERTIES LANGUAGE C )

include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_INCLUDE_DIR}")


add_library(${PROJECT_NAME} SHARED ${CROW_SRCS})
#target_link_libraries(${PROJECT_NAME} tcmalloc)
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-Wall -std=c++1y")
