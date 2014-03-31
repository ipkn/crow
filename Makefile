all: example
example: example.cpp flask.h http_server.h http_connection.h parser.h
	g++ -g -std=c++11 -o example example.cpp http-parser/http_parser.c -pthread -lboost_system -lboost_thread -I http-parser/
test: example
	./example &
	test.py
	pkill example
