all: covtest example
example: example.cpp flask.h http_server.h http_connection.h parser.h http_response.h routing.h common.h
	g++ -Wall -g -O2 -std=c++11 -o example example.cpp http-parser/http_parser.c -pthread -lboost_system -lboost_thread -I http-parser/

test: covtest

runtest: example
	pkill example || exit 0
	./example &
	python test.py || exit 0
	pkill example

unittest: unittest.cpp routing.h
	g++ -Wall -g -O2 -std=c++11 -o unittest unittest.cpp
	./unittest

covtest: unittest.cpp routing.h
	g++ -Wall -g -O2 -std=c++11 --coverage -o covtest unittest.cpp -fkeep-inline-functions -fno-default-inline -fno-inline-small-functions
	./covtest
	gcov -r unittest.cpp


