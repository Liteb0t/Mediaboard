cmake_minimum_required(VERSION 3.23)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend)
set(frontend_files
  change_password.html
  index.html
  manage_permissions.html
  manage_server.html
  registration.html
  webrtc.html
  UserAuthentication.js
  permissions.js
)
set(frontend_files_out
  ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/change_password.html
  ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/index.html
  ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/manage_permissions.html
  ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/manage_server.html
  ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/registration.html
  ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/webrtc.html
  ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/UserAuthentication.js
  ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/permissions.js
)
add_custom_target(frontend
  # COMMAND m4 _registration.html >${CMAKE_BINARY_DIR}/registration.html
  WORKING_DIRECTORY
    ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend
  DEPENDS
    ${frontend_files_out}
    static
  VERBATIM
)
foreach(frontend_file IN LISTS frontend_files)
  message("frontend_file: ${frontend_file}")
  add_custom_command(
    OUTPUT
      ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/${frontend_file}
    COMMAND
      # TODO Possibly replace m4 with CMake configure_file
      # https://cmake.org/cmake/help/latest/command/configure_file.html
      # https://stackoverflow.com/a/34800667/18658154
      m4 ${CMAKE_CURRENT_SOURCE_DIR}/_${frontend_file} >${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/${frontend_file}
    DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/_${frontend_file}
      ${CMAKE_CURRENT_SOURCE_DIR}/tokens.m4
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  install(FILES ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/${frontend_file}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/FuzeMediaboard/frontend)
endforeach()
add_custom_command(
  TARGET frontend POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_CURRENT_SOURCE_DIR}/static
      ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/static
)
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/static
  DESTINATION ${CMAKE_INSTALL_DATADIR}/FuzeMediaboard/frontend
)
