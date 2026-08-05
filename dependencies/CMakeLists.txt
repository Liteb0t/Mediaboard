cmake_minimum_required(VERSION 3.23)

option(FUZEHTTP_EXAMPLE "" OFF)
add_subdirectory(FuzeHttp)
if(WITH_WEBRTC)
  option(NO_EXAMPLES "" ON) # Removes dependence on nlohmann::json
  option(NO_TESTS "" ON)
  add_subdirectory(libdatachannel)
endif()
