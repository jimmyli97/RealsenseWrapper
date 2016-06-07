cmake_minimum_required(VERSION 3.5.2)
set (BUILD_SHARED_LIBS FALSE)

project (rswrapper)

file (GLOB SOURCES "src/*.cpp" "src/*.h")

add_executable (rswrapper ${SOURCES})

# windows, very non portable (replace with find_library script later...) .lib
# build the .dll and the .lib by opening the librealsense project in VS
include_directories (${CMAKE_SOURCE_DIR}/librealsense/include/librealsense)
target_link_libraries (rswrapper "${CMAKE_SOURCE_DIR}/librealsense/bin/x64/realsense-d.lib")

