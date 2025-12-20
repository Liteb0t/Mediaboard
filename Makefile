CFLAGS=-O0

all: index.html registration.html change_password.html manage_server.html server

index.html: _index.html tokens.m4
	m4 _index.html >index.html

registration.html: _registration.html tokens.m4
	m4 _registration.html >registration.html

change_password.html: _change_password.html tokens.m4
	m4 _change_password.html >change_password.html

manage_server.html: _manage_server.html tokens.m4
	m4 _manage_server.html >manage_server.html

server: main.o listener.o http_session.o shared_state.o websocket_session.o db_interface.o post.o thread.o board.o group.o user.o permission_managed_object.o
	g++ $(CFLAGS) `Magick++-config --cxxflags --cppflags` -o server main.o listener.o http_session.o shared_state.o websocket_session.o db_interface.o post.o thread.o board.o group.o user.o permission_managed_object.o -I/usr/include/postgresql -l:libecpg.so -l:libpgtypes.so `Magick++-config --ldflags --libs` -lboost_program_options -lboost_filesystem -lboost_system

main.o: main.cpp http_session.o
	g++ $(CFLAGS) -c `Magick++-config --cxxflags --cppflags` main.cpp -o main.o `Magick++-config --ldflags --libs` -lboost_program_options -lboost_filesystem -lboost_system

listener.o: listener.cpp listener.hpp
	g++ $(CFLAGS) -c listener.cpp -o listener.o

http_session.o: http_session.cpp http_session.hpp field_lengths.h
	g++ $(CFLAGS) -c `Magick++-config --cxxflags --cppflags` http_session.cpp -o http_session.o `Magick++-config --ldflags --libs` -lboost_filesystem -lboost_system

shared_state.o: shared_state.cpp shared_state.hpp
	g++ $(CFLAGS) -c shared_state.cpp -o shared_state.o

permission_managed_object.o: permission_managed_object.cpp permission_managed_object.hpp permission_collection.hpp permission_setting.hpp
	g++ $(CFLAGS) -c permission_managed_object.cpp -o permission_managed_object.o

user.o: user.cpp user.hpp
	g++ $(CFLAGS) -c user.cpp -o user.o

group.o: group.cpp group.hpp
	g++ $(CFLAGS) -c group.cpp -o group.o

websocket_session.o: websocket_session.cpp websocket_session.hpp
	g++ $(CFLAGS) -c websocket_session.cpp -o websocket_session.o

board.o: board.cpp board.hpp 
	g++ $(CFLAGS) -c board.cpp -o board.o

thread.o: thread.cpp thread.hpp 
	g++ $(CFLAGS) -c thread.cpp -o thread.o

post.o: post.cpp post.hpp field_lengths.h
	g++ $(CFLAGS) -c post.cpp -o post.o

db_interface.o: db_interface.c db_interface.h
	gcc $(CFLAGS) -c db_interface.c -o db_interface.o -I/usr/include/postgresql -lecpg

db_interface.c: db_interface.pgc field_lengths.h
	ecpg db_interface.pgc
