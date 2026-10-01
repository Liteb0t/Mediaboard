# Copyright (c) 2026, Fuze.page
# Fuze Human-oriented License v1
cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

project(Mediaboard
  VERSION 0.2.3
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
option(WITH_ASAN "Build with AddressSanitizer" OFF)
if(WITH_ASAN)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()
include(GNUInstallDirs)

# Find dependencies sneeded by the project
cmake_policy(SET CMP0167 NEW) # use BoostConfig.cmake instead of legacy FindBoost, for CMake < 3.30
find_package(Boost 1.88 COMPONENTS json program_options REQUIRED)

set(data_destination "usr/share")
add_subdirectory(Manager)
add_subdirectory(Server)
add_subdirectory(assets)
# add_subdirectory(frontend)
add_subdirectory(dependencies)

# unset(FUZEDBI_USE_POSTGRES CACHE)
