cmake_minimum_required(VERSION 3.23)

project(Mediaboard
  VERSION 0.0.6
  DESCRIPTION "Chat server with file-sharing and advanced permissions"
  LANGUAGES C CXX
  HOMEPAGE_URL "https://fuze.page/software/mediaboard"
)

find_package(Boost 1.81 COMPONENTS program_options filesystem REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ImageMagick COMPONENTS Magick++ REQUIRED)
add_definitions( -DMAGICKCORE_QUANTUM_DEPTH=16 )
add_definitions( -DMAGICKCORE_HDRI_ENABLE=0 )
include_directories(${ImageMagick_INCLUDE_DIRS})
find_library(ECPG_LIB ecpg)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread") # Required on FreeBSD

add_executable(MediaboardServer)
target_compile_features(MediaboardServer INTERFACE cxx_std_17)
target_link_libraries(MediaboardServer
  PRIVATE
    Boost::filesystem
    Boost::program_options
    nlohmann_json::nlohmann_json
    ${ImageMagick_LIBRARIES}
    ${ECPG_LIB}
)
target_sources(MediaboardServer
  PRIVATE
    board.cpp
    db_interface.c
    group.cpp
    http_session.cpp
    listener.cpp
    main.cpp
    permission_managed_object.cpp
    post.cpp
    shared_state.cpp
    thread.cpp
    user.cpp
    websocket_session.cpp
)
configure_file(config.ini ${CMAKE_CURRENT_BINARY_DIR}/ COPYONLY)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/db_interface.c
  COMMAND ecpg ${CMAKE_CURRENT_SOURCE_DIR}/db_interface.pgc
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/db_interface.pgc
)
add_dependencies(MediaboardServer
  frontend
)
add_subdirectory(frontend)
