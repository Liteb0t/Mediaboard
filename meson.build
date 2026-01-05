project('Mediaboard', 'c', 'cpp', default_options: ['default_library=static', 'cpp_std=c++17'], version : '0.0.5')
# >>

# sources = ['main.cpp', 'shared_state.cpp', 'thread.cpp', 'websocket_session.cpp', 'listener.cpp']
do_ecpg = run_command('bash', 'build.sh')
# db_interface = library('db_interface', 'db_interface.c', dependencies : dependency('libecpg'))
# user = library('user', 'user.cpp', link_with : db_interface)
# group = library('group', 'group.cpp', link_with : db_interface, dependencies : dependency('nlohmann_json'))
# permission_managed_object = library('permission_managed_object', 'permission_managed_object.cpp', link_with : [db_interface, user, group], dependencies : dependency('nlohmann_json'))
# 
# post = library('post', 'post.cpp', link_with : db_interface, dependencies : dependency('nlohmann_json'))
# thread = library('thread', 'thread.cpp', link_with : [db_interface, permission_managed_object, post], dependencies : dependency('nlohmann_json'))
# board = library('board', 'board.cpp', link_with : [thread, db_interface, permission_managed_object], dependencies : dependency('nlohmann_json'))
# websocket_session = library('websocket_session', 'websocket_session.cpp', link_with: [board], dependencies : dependency('boost'))
# shared_state = library('shared_state', 'shared_state.cpp', link_with : [permission_managed_object, websocket_session, board], dependencies : [dependency('boost'), dependency('nlohmann_json')])
# http_session = library('http_session', 'http_session.cpp', link_with : [shared_state], dependencies : [dependency('boost'), dependency('Magick++')])
# listener = library('listener', 'listener.cpp', link_with : http_session, dependencies : [dependency('boost'), dependency('Magick++')])
deps = [dependency('boost', modules : ['program_options', 'filesystem']),
        dependency('Magick++'),
        dependency('nlohmann_json'),
	dependency('libecpg')]
# executable('server', 'main.cpp', link_with : [listener, shared_state, websocket_session], dependencies : deps)
executable('server',
	'main.cpp',
	'websocket_session.cpp',
	'shared_state.cpp',
	'http_session.cpp',
	'listener.cpp',
	'board.cpp',
	'thread.cpp',
	'post.cpp',
	'permission_managed_object.cpp',
	'group.cpp',
	'user.cpp',
	'db_interface.c',
	dependencies : deps)
