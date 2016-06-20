cmake_minimum_required(VERSION 3.5.2)
set (BUILD_SHARED_LIBS FALSE)

project (rswrapper)

file (GLOB SOURCES "src/*.cpp" "src/*.h")

set (Boost_USE_STATIC_LIBS ON)
find_package (Boost COMPONENTS filesystem REQUIRED)


# For librealsense, build the .dll and the .lib yourself, then place in external folder
include_directories (${CMAKE_SOURCE_DIR}/external/include/ 
					 ${Boost_INCLUDE_DIR} )

add_executable (rswrapper ${SOURCES})

IF(WIN32)
target_link_libraries (rswrapper "opengl32.lib" "${CMAKE_SOURCE_DIR}/external/lib/win32/realsense-d.lib" ${Boost_LIBRARIES} "${CMAKE_SOURCE_DIR}/external/lib/win32/glfw3dll.lib" "${CMAKE_SOURCE_DIR}/external/lib/win32/glfw3.lib")

# Copy DLL over for postbuild
add_custom_command(TARGET rswrapper POST_BUILD 
	 COMMAND ${CMAKE_COMMAND} -E copy
		 "${CMAKE_SOURCE_DIR}/external/lib/win32/realsense-d.dll"
		 "${CMAKE_SOURCE_DIR}/external/lib/win32/glfw3.dll"
		 "$<TARGET_FILE_DIR:rswrapper>/"
     )
ENDIF(WIN32)