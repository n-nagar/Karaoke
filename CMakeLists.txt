cmake_minimum_required(VERSION 2.8)
project(CDGParser)

SET(CMAKE_CXX_FLAGS "-pthread -std=c++11")

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
pkg_search_module(X11 REQUIRED glfw3)

include_directories(/home/nnagar/git/FMOD/api/lowlevel/inc)

add_executable(CDGParser CDGParser.cpp GraphicCDG.cpp FMODAudio.cpp FileIO.cpp)

target_link_libraries(CDGParser ${GLFW_STATIC_LIBRARIES})
target_link_libraries(CDGParser fmod)
