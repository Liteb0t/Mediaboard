# Copyright (c) 2026, Fuze.page
# Fuze Human-oriented License v1
cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

add_executable(MediaboardManager)

target_link_libraries(MediaboardManager
  PRIVATE
    Boost::json
    Boost::program_options
    FuzeHttp
)
target_sources(MediaboardManager
  PRIVATE
    main.cpp
    migrations.cpp
  PUBLIC
    FILE_SET CXX_MODULES FILES
      Config.cppm
      HomeDirLibrary.cppm
      resolvers.cppm
      State.cppm
      urls.cppm
      views.cppm
)
set(frontend_list "") ## its a list which will be used as DEPENDS on add_custom_target
set(FRONTEND_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/frontend)
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
    set(qualified_destination ${CMAKE_BINARY_DIR}/share/MediaboardManager/frontend/${file})
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
    set(qualified_destination ${CMAKE_BINARY_DIR}/share/MediaboardManager/frontend/${file})
    add_custom_command(
      OUTPUT ${qualified_destination}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FRONTEND_SOURCE_DIR}/${file} ${qualified_destination}
      DEPENDS ${FRONTEND_SOURCE_DIR}/${file}
      COMMENT "Copying ${FRONTEND_SOURCE_DIR}/${file}"
    )
    list(APPEND frontend_list ${qualified_destination})
  endif()
endforeach()

add_custom_target(MediaboardManagerfrontend ALL
  DEPENDS
    ${frontend_list}
)
set(current_data_destination ${data_destination}/MediaboardManager)
install(
  TARGETS MediaboardManager RUNTIME
  DESTINATION usr/bin
)
install(
  DIRECTORY ${CMAKE_BINARY_DIR}/share/MediaboardManager/frontend
  DESTINATION ${current_data_destination}
)
configure_file(config.ini ${CMAKE_BINARY_DIR}/share/MediaboardManager/config.ini COPYONLY)
configure_file(database_template.sql ${CMAKE_BINARY_DIR}/share/MediaboardManager/database_template.sql COPYONLY)
add_custom_command(TARGET MediaboardManager POST_BUILD
  COMMAND ${CMAKE_BINARY_DIR}/bin/MediaboardManager --generate_manifest
  COMMENT "Generating manifest.json"
)
install(
  FILES
    ${CMAKE_BINARY_DIR}/share/MediaboardManager/manifest.json
  DESTINATION ${current_data_destination}
  OPTIONAL
)
install(
  FILES
    config.ini
    database_template.sql
  DESTINATION ${current_data_destination}
)
add_dependencies(MediaboardManager
  MediaboardManagerfrontend
)
