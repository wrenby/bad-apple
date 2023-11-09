cmake_minimum_required(VERSION 3.10)

SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED true)

project(badapple)
add_executable(badapple main.cpp)

set(CURSES_NEED_NCURSES true)
find_package(Curses REQUIRED)

find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(libvlc REQUIRED)
add_dependencies(badapple OpenGL::OpenGL OpenGL::EGL OpenGL::GLU GLEW::GLEW glm)

include_directories(${CURSES_INCLUDE_DIRS} ${LIBVLC_INCLUDE_DIR})

target_link_libraries(badapple OpenGL::OpenGL OpenGL::EGL OpenGL::GLU GLEW::GLEW glm ${CURSES_LIBRARIES} ${LIBVLC_LIBRARY})