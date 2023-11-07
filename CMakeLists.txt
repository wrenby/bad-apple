cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 14)
set(CXX_STANDARD_REQUIRED true)

project(donut)
add_executable(donut main.cpp)

set(CURSES_NEED_NCURSES true)
find_package(Curses 6.1 REQUIRED) # version 6.1 introduced direct color

find_package(OpenGL REQUIRED COMPONENTS OpenGL)
find_package(GLUT REQUIRED COMPONENTS GLUT) # almost exclusively using glut for the glutSolidTorus function
find_package(glm REQUIRED)
add_dependencies(donut OpenGL::OpenGL GLUT::GLUT glm)

include_directories(${CURSES_INCLUDE_DIRS})

target_link_libraries(donut OpenGL::OpenGL GLUT::GLUT glm ${CURSES_LIBRARIES})