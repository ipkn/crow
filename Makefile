PLATFORM ?= $(shell sh -c 'uname -s | tr "[A-Z]" "[a-z]"')

ifeq ($(PLATFORM),darwin)
FLAGS_BOOST_THREAD = -lboost_thread-mt
else
FLAGS_BOOST_THREAD = -lboost_thread
FLAGS_DEBUG = -g
FLAGS_GCOV = -r
endif

binaries=covtest example example_chat

all: covtest example example_chat

example_chat: example_chat.cpp settings.h crow.h http_server.h http_connection.h parser.h http_response.h routing.h common.h utility.h json.h datetime.h logging.h mustache.h
	${CXX} -Wall $(FLAGS_DEBUG) -std=c++1y -o example_chat example_chat.cpp http-parser/http_parser.c -pthread -lboost_system $(FLAGS_BOOST_THREAD) -I http-parser/


example: example.cpp settings.h crow.h http_server.h http_connection.h parser.h http_response.h routing.h common.h utility.h json.h datetime.h logging.h mustache.h
	${CXX} -Wall $(FLAGS_DEBUG) -O3 -std=c++1y -o example example.cpp http-parser/http_parser.c -pthread -lboost_system $(FLAGS_BOOST_THREAD) -ltcmalloc_minimal -I http-parser/

test: covtest

runtest: example
	pkill example || exit 0
	./example &
	python test.py || exit 0
	pkill example

unittest: unittest.cpp routing.h utility.h crow.h http_server.h http_connection.h parser.h http_response.h common.h json.h datetime.h logging.h
	${CXX} -Wall $(FLAGS_DEBUG) -std=c++1y -o unittest unittest.cpp http-parser/http_parser.c -pthread -lboost_system $(FLAGS_BOOST_THREAD) -I http-parser/
	./unittest

covtest: unittest.cpp routing.h utility.h crow.h http_server.h http_connection.h parser.h http_response.h common.h json.h datetime.h logging.h mustache.h
	${CXX} -Wall $(FLAGS_DEBUG) -std=c++1y -o covtest --coverage unittest.cpp http-parser/http_parser.c -pthread -lboost_system $(FLAGS_BOOST_THREAD) -I http-parser/
	./covtest
	gcov $(FLAGS_GCOV) unittest.cpp

.PHONY: clean

clean:
	rm -f $(binaries) *.o
