cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

project(Mediaboard
  VERSION 0.2
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
option(WITH_WEBRTC "Compile with libdatachannel" ON)
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
  target_compile_definitions(MediaboardServer PRIVATE WITH_MAGICK)
  target_link_libraries(MediaboardServer PRIVATE ${ImageMagick_LIBRARIES})
else()
  message("Compiling without ImageMagick support. Certain features such as thumbnail creation will not work.")
endif()

if(WITH_WEBRTC)
  target_compile_definitions(MediaboardServer PRIVATE WITH_WEBRTC)
  target_link_libraries(MediaboardServer PRIVATE datachannel-static)
  target_sources(MediaboardServer PUBLIC FILE_SET CXX_MODULES FILES Room.cppm)
endif()
target_link_libraries(MediaboardServer
  PRIVATE
    Boost::json
    Boost::program_options
    FuzeHttp
)
target_sources(MediaboardServer
  PRIVATE
    main.cpp
    migrations.cpp
    urls.cpp
    # views.cpp
  PUBLIC
    FILE_SET CXX_MODULES FILES
      Board.cppm
      MediaboardWebsocketSession.cppm
      Message.cppm
      Permission.cppm
      State.cppm
      Thread.cppm
      views.cppm
      views_media.cppm
      views_registration.cppm
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
set(FRONTEND_SOURCE_DIR ${CMAKE_SOURCE_DIR}/frontend)
# file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend)
file(GLOB_RECURSE frontend_files CONFIGURE_DEPENDS RELATIVE ${FRONTEND_SOURCE_DIR} ${FRONTEND_SOURCE_DIR}/*)
# \.template\.[^\\\/]+$
foreach(file ${frontend_files})
  message("Checking file ${file}")
  if(file MATCHES "^include/") # macro sources aint included in final doctree
    message("----skipping (is in include directory)")
    continue()
  elseif (file MATCHES "\.template\.[^\\\/]+$")
    message("----is template")
    set(frontend_definitions "")
    if (WITH_WEBRTC)
      set(frontend_definitions "-DWITH_WEBRTC=1")
    endif()
    # [AI glasnost] this section assisted by Claude Sonnet 5
    set(qualified_source ${FRONTEND_SOURCE_DIR}/${file})
    set(qualified_destination ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/${file})
    set(depfile ${CMAKE_BINARY_DIR}/frontend_deps/${file}.d)

    get_filename_component(destination_dir ${qualified_destination} DIRECTORY)
    get_filename_component(depfile_dir ${depfile} DIRECTORY)
    file(MAKE_DIRECTORY ${destination_dir})
    file(MAKE_DIRECTORY ${depfile_dir})

    # Extract template name without .template.* suffix
    string(REGEX REPLACE "\.template\.[^\\\/]+$" "" template_basename "${file}")
    # Convert to uppercase token: index.html → INDEX
    string(TOUPPER "${template_basename}" template_token)
    string(REGEX REPLACE "[^A-Z0-9_]" "_" template_token "${template_token}")

    add_custom_command(
      OUTPUT ${qualified_destination}
      COMMAND ${CMAKE_C_COMPILER} # runs C preprocessor
              -E -P -CC -undef -nostdinc -x c -Wno-trigraphs -Wno-c23-extensions
              -D${template_token} ${frontend_definitions}
              -I ${FRONTEND_SOURCE_DIR} -I ${FRONTEND_SOURCE_DIR}/include
              -MMD -MF ${depfile} -MT ${qualified_destination}
              ${qualified_source} -o ${qualified_destination}
      DEPENDS ${qualified_source}
      DEPFILE ${depfile}
      COMMENT "Preprocessing ${file} :: ${template_token}"
      VERBATIM
    )
    list(APPEND frontend_list ${qualified_destination})
    # [AI glasnost] end AI-assisted section
  else()
    message("----is regular file")
    set(qualified_destination ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/${file})
    add_custom_command(
      OUTPUT ${qualified_destination}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FRONTEND_SOURCE_DIR}/${file} ${qualified_destination}
      DEPENDS ${FRONTEND_SOURCE_DIR}/${file}
      COMMENT "Copying ${FRONTEND_SOURCE_DIR}/${file}"
    )
    list(APPEND frontend_list ${qualified_destination})
  endif()
endforeach()

add_custom_target(frontend ALL
  DEPENDS
    ${frontend_list}
)
install(
  DIRECTORY ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend
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
