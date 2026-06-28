cmake_minimum_required(VERSION 3.23)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend)
add_custom_target(frontend
  # COMMAND m4 _registration.html >${CMAKE_BINARY_DIR}/registration.html
  WORKING_DIRECTORY
    ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/static
  VERBATIM
)
add_custom_command(
  TARGET frontend POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend
)
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/static
  DESTINATION ${CMAKE_INSTALL_DATADIR}/FuzeMediaboard/frontend
)
