all: index.html registration.html server

index.html: _index.html tokens.m4
	m4 _index.html >index.html

registration.html: _registration.html tokens.m4
	m4 _registration.html >registration.html

server: main.o listener.o http_session.o shared_state.o websocket_session.o db_interface.o post.o thread.o board.o 
	g++ `Magick++-config --cxxflags --cppflags` -o server main.o listener.o http_session.o shared_state.o websocket_session.o db_interface.o post.o thread.o board.o -I/usr/include/postgresql -l:libecpg.so -l:libpgtypes.so `Magick++-config --ldflags --libs` -lboost_program_options -lboost_filesystem -lboost_system

main.o: main.cpp 
	g++ -c `Magick++-config --cxxflags --cppflags` main.cpp -o main.o `Magick++-config --ldflags --libs` -lboost_program_options -lboost_filesystem -lboost_system

listener.o: listener.cpp listener.hpp
	g++ -c listener.cpp -o listener.o

http_session.o: http_session.cpp http_session.hpp 
	g++ -c `Magick++-config --cxxflags --cppflags` http_session.cpp -o http_session.o `Magick++-config --ldflags --libs` -lboost_filesystem -lboost_system

shared_state.o: shared_state.cpp shared_state.hpp 
	g++ -c shared_state.cpp -o shared_state.o

websocket_session.o: websocket_session.cpp websocket_session.hpp 
	g++ -c websocket_session.cpp -o websocket_session.o

board.o: board.cpp board.hpp 
	g++ -c board.cpp -o board.o

thread.o: thread.cpp thread.hpp 
	g++ -c thread.cpp -o thread.o

post.o: post.cpp post.hpp field_lengths.h
	g++ -c post.cpp -o post.o

db_interface.o: db_interface.c db_interface.h
	gcc -c db_interface.c -o db_interface.o -I/usr/include/postgresql -lecpg

db_interface.c: db_interface.pgc field_lengths.h
	ecpg db_interface.pgc
