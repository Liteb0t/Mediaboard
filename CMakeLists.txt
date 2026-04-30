cmake_minimum_required(VERSION 3.23 FATAL_ERROR)

project(Mediaboard
  VERSION 0.1
  DESCRIPTION "Chat server with file-sharing and advanced permissions"
  LANGUAGES C CXX
  HOMEPAGE_URL "https://fuze.page/software/mediaboard"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread") # Required on FreeBSD
set(CMAKE_EXPORT_COMPILE_COMMANDS 1) # writes compile_commands.json for clangd to use
set(SODIUM_DISABLE_TESTS ON)

# Find dependencies sneeded by the project
cmake_policy(SET CMP0167 NEW) # use BoostConfig.cmake instead of legacy FindBoost, for CMake < 3.30
find_package(Boost 1.88 COMPONENTS filesystem json program_options REQUIRED)

find_package(ImageMagick COMPONENTS Magick++ REQUIRED)
add_definitions( -DMAGICKCORE_QUANTUM_DEPTH=16 )
add_definitions( -DMAGICKCORE_HDRI_ENABLE=0 )
include_directories(${ImageMagick_INCLUDE_DIRS})

find_package(PostgreSQL REQUIRED)
find_library(ECPG_LIB ecpg)
include_directories (${PostgreSQL_INCLUDE_DIRS})
link_directories(${PostgreSQL_LIBRARY_DIRS})

find_package(SQLite3) # CMake >= 4.3
if(NOT TARGET SQLite3::SQLite3) # CMake < 4.3
  add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif()

add_executable(MediaboardServer)
target_link_libraries(MediaboardServer
  PRIVATE
    Boost::filesystem
    Boost::json
    Boost::program_options
    ${ImageMagick_LIBRARIES}
    ${ECPG_LIB}
    PostgreSQL::PostgreSQL
    SQLite3::SQLite3
    sodium
)
target_sources(MediaboardServer
  PRIVATE
    Board.cpp
    db_interface.c
    FuzeHttp.cpp
    Group.cpp
    http_session.cpp
    listener.cpp
    main.cpp
    Message.cpp
    permission_managed_object.cpp
    shared_state.cpp
    Thread.cpp
    urls.cpp
    views.cpp
    views_registration.cpp
    websocket_session.cpp
)
configure_file(config.ini ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
configure_file(database_template.sql ${CMAKE_CURRENT_BINARY_DIR}/database/database_template.sql COPYONLY)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/db_interface.c
  COMMAND ecpg ${CMAKE_CURRENT_SOURCE_DIR}/db_interface.pgc
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/db_interface.pgc
)
add_dependencies(MediaboardServer
  frontend
)
add_subdirectory(frontend)
add_subdirectory(dependencies)

install(
  TARGETS MediaboardServer
  RUNTIME DESTINATION bin
)
