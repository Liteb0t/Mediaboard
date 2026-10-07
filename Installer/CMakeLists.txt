# Copyright (c) 2026, Fuze.page
# Fuze Human-oriented License v1
cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

add_executable(MediaboardInstaller)

target_sources(MediaboardInstaller
  PRIVATE
    main.cpp
  PUBLIC
    FILE_SET CXX_MODULES FILES
      HomeDirLibrary.cppm
      Installer.cppm
)
install(
  TARGETS MediaboardInstaller RUNTIME
  DESTINATION usr/bin
)
