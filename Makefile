all: example
example: example.cpp flask.h http_server.h http_connection.h parser.h http_response.h
	g++ -g -std=c++11 -o example example.cpp http-parser/http_parser.c -pthread -lboost_system -lboost_thread -I http-parser/
test: example
	pkill example || exit 0
	./example &
	python test.py || exit 0
	pkill example
