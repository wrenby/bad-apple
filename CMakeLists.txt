cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED true)

project(donut)
add_executable(donut main.cpp)

set(CURSES_NEED_NCURSES true)
find_package(Curses REQUIRED)

find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
add_dependencies(donut OpenGL::OpenGL OpenGL::EGL OpenGL::GLU GLEW::GLEW glm)

include_directories(${CURSES_INCLUDE_DIRS})

target_link_libraries(donut OpenGL::OpenGL OpenGL::EGL OpenGL::GLU GLEW::GLEW glm ${CURSES_LIBRARIES})