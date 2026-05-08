cmake_minimum_required(VERSION 3.23)

set(frontend_files
  api.js
  change_password.html
  index.html
  manage_permissions.html
  manage_server.html
  permissions.js
  registration.html
)
add_custom_target(frontend
  # COMMAND m4 _registration.html >${CMAKE_BINARY_DIR}/registration.html
  WORKING_DIRECTORY
    ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS
    ${frontend_files}
    static
  VERBATIM
)
foreach(frontend_file IN LISTS frontend_files)
  message("frontend_file: ${frontend_file}")
  add_custom_command(
    OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/${frontend_file}
    COMMAND
      # TODO Possibly replace m4 with CMake configure_file
      # https://cmake.org/cmake/help/latest/command/configure_file.html
      # https://stackoverflow.com/a/34800667/18658154
      m4 ${CMAKE_CURRENT_SOURCE_DIR}/_${frontend_file} >${CMAKE_CURRENT_BINARY_DIR}/${frontend_file}
    DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/_${frontend_file}
      ${CMAKE_CURRENT_SOURCE_DIR}/tokens.m4
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${frontend_file}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/frontend)
endforeach()
add_custom_command(
  TARGET frontend POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_CURRENT_SOURCE_DIR}/static
      ${CMAKE_CURRENT_BINARY_DIR}/static
)
