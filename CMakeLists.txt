cmake_minimum_required(VERSION 3.23 FATAL_ERROR)

project(Mediaboard
  VERSION 0.1.4
  DESCRIPTION "Chat server with file-sharing and advanced permissions"
  LANGUAGES C CXX
  HOMEPAGE_URL "https://fuze.page/software/mediaboard"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread") # Required on FreeBSD
set(CMAKE_EXPORT_COMPILE_COMMANDS 1) # writes compile_commands.json for clangd to use
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(SODIUM_DISABLE_TESTS ON)
option(FUZEDBI_USE_POSTGRES "Use PostgreSQL interface instead of SQLite" OFF)
option(WITH_MAGICK "Compile with ImageMagick support" ON)
include(GNUInstallDirs)

# Find dependencies sneeded by the project
cmake_policy(SET CMP0167 NEW) # use BoostConfig.cmake instead of legacy FindBoost, for CMake < 3.30
find_package(Boost 1.88 COMPONENTS json program_options REQUIRED)

add_executable(MediaboardServer)
if(WITH_MAGICK)
  find_package(ImageMagick COMPONENTS Magick++ REQUIRED)
  add_compile_definitions( -DMAGICKCORE_QUANTUM_DEPTH=16 )
  add_compile_definitions( -DMAGICKCORE_HDRI_ENABLE=0 )
  include_directories(${ImageMagick_INCLUDE_DIRS})
  target_compile_definitions(MediaboardServer PUBLIC WITH_MAGICK)
  target_link_libraries(MediaboardServer PRIVATE ${ImageMagick_LIBRARIES})
else()
  message("Compiling without ImageMagick support. Certain features such as thumbnail creation will not work.")
endif()
if(FUZEDBI_USE_POSTGRES)
  message("FuzeDBI will use PostgreSQL interface")

  find_package(PostgreSQL REQUIRED)
  include_directories (${PostgreSQL_INCLUDE_DIRS})
  link_directories(${PostgreSQL_LIBRARY_DIRS})

  target_compile_definitions(MediaboardServer PUBLIC FUZEDBI_POSTGRES)
  target_compile_definitions(MediaboardServer PUBLIC FUZEDBI_DB="postgresql")
  target_link_libraries(MediaboardServer PRIVATE PostgreSQL::PostgreSQL)
else()
  message("FuzeDBI will use SQLite interface")

  find_package(SQLite3) # CMake >= 4.3
  if(NOT TARGET SQLite3::SQLite3) # CMake < 4.3
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
  endif()

  target_compile_definitions(MediaboardServer PUBLIC FUZEDBI_SQLITE)
  target_compile_definitions(MediaboardServer PUBLIC FUZEDBI_DB="sqlite")
  target_link_libraries(MediaboardServer PRIVATE SQLite3::SQLite3)
endif()
target_link_libraries(MediaboardServer
  PRIVATE
    Boost::json
    Boost::program_options
    sodium
)
target_sources(MediaboardServer
  PRIVATE
    Board.cpp
    FuzeHttp.cpp
    FuzeHttpState.cpp
    FuzeHttpServer.cpp
    FuzeHttpUtils.cpp
    Group.cpp
    # http_session.cpp
    # Listener.cpp
    main.cpp
    Message.cpp
    migrations.cpp
    PermissionObject.cpp
    shared_state.cpp
    Thread.cpp
    urls.cpp
    views.cpp
    views_media.cpp
    views_registration.cpp
    # WebsocketSession.cpp
)
configure_file(config.ini ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/config.ini COPYONLY)
configure_file(database_template.sql ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/database_template.sql COPYONLY)
install(
  FILES
    config.ini
    database_template.sql
  DESTINATION ${CMAKE_INSTALL_DATADIR}/FuzeMediaboard
)
set(frontend_list "") ## its a list which will be used as DEPENDS on add_custom_target
# file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend)
file(GLOB_RECURSE frontend_files CONFIGURE_DEPENDS RELATIVE ${CMAKE_SOURCE_DIR}/frontend ${CMAKE_SOURCE_DIR}/frontend/*)
foreach(file ${frontend_files})
  set(destination ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/${file})
  add_custom_command(
    OUTPUT ${destination}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/frontend/${file} ${destination}
    DEPENDS ${CMAKE_SOURCE_DIR}/frontend/${file}
    COMMENT "Copying ${CMAKE_SOURCE_DIR}/frontend/${file}"
  )
  list(APPEND frontend_list ${destination})
endforeach()

add_custom_target(frontend ALL
  DEPENDS
    ${frontend_list}
)
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/frontend
  DESTINATION ${CMAKE_INSTALL_DATADIR}/FuzeMediaboard/
)
add_dependencies(MediaboardServer
  frontend
)
add_subdirectory(assets)
# add_subdirectory(frontend)
add_subdirectory(dependencies)

install(
  TARGETS MediaboardServer
  FILE_SET HEADERS
  # RUNTIME DESTINATION bin
)
# unset(FUZEDBI_USE_POSTGRES CACHE)
