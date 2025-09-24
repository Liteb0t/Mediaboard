all: index.html server

index.html: _index.html tokens.m4
	m4 _index.html >index.html

server: main.o
	g++ `Magick++-config --cxxflags --cppflags` -o server main.o listener.o http_session.o shared_state.o websocket_session.o db_interface.o post.o thread.o board.o -I/usr/include/postgresql -l:libecpg.so -l:libpgtypes.so `Magick++-config --ldflags --libs` -lboost_program_options

main.o: main.cpp listener.o shared_state.o
	g++ -c `Magick++-config --cxxflags --cppflags` main.cpp -o main.o `Magick++-config --ldflags --libs` -lboost_program_options

listener.o: listener.cpp listener.hpp http_session.o
	g++ -c listener.cpp -o listener.o

http_session.o: http_session.cpp http_session.hpp websocket_session.o
	g++ -c `Magick++-config --cxxflags --cppflags` http_session.cpp -o http_session.o `Magick++-config --ldflags --libs`

shared_state.o: shared_state.cpp shared_state.hpp websocket_session.o
	g++ -c shared_state.cpp -o shared_state.o

websocket_session.o: websocket_session.cpp websocket_session.hpp board.o
	g++ -c websocket_session.cpp -o websocket_session.o

board.o: board.cpp board.hpp thread.o
	g++ -c board.cpp -o board.o

thread.o: thread.cpp thread.hpp post.o
	g++ -c thread.cpp -o thread.o

post.o: post.cpp post.hpp db_interface.o
	g++ -c post.cpp -o post.o

db_interface.o: db_interface.c db_interface.h
	gcc -c db_interface.c -o db_interface.o -I/usr/include/postgresql -lecpg

db_interface.c: db_interface.pgc
	ecpg db_interface.pgc
