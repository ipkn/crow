binaries=covtest example

all: covtest example

example: example.cpp settings.h crow.h http_server.h http_connection.h parser.h http_response.h routing.h common.h utility.h json.h datetime.h logging.h
	g++ -Wall -g -O3 -std=c++1y -o example example.cpp http-parser/http_parser.c -pthread -lboost_system -lboost_thread -ltcmalloc_minimal -I http-parser/

test: covtest

runtest: example
	pkill example || exit 0
	./example &
	python test.py || exit 0
	pkill example

unittest: unittest.cpp routing.h utility.h crow.h http_server.h http_connection.h parser.h http_response.h common.h json.h datetime.h logging.h
	g++ -Wall -g -std=c++1y -o unittest unittest.cpp http-parser/http_parser.c -pthread -lboost_system -lboost_thread -I http-parser/
	./unittest

covtest: unittest.cpp routing.h utility.h crow.h http_server.h http_connection.h parser.h http_response.h common.h json.h datetime.h logging.h
	g++ -Wall -g -std=c++1y -o covtest --coverage unittest.cpp http-parser/http_parser.c -pthread -lboost_system -lboost_thread -I http-parser/
	./covtest
	gcov -r unittest.cpp

.PHONY: clean

clean:
	rm -f $(binaries) *.o
