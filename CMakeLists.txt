cmake_minimum_required(VERSION 3.5.2)
project (rswrapper)

file (GLOB SOURCES "src/*.cpp" "src/*.h")

add_executable (rswrapper ${SOURCES})
