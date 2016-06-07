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
target_link_libraries (rswrapper "${CMAKE_SOURCE_DIR}/external/lib/win32_realsense-d.lib" ${Boost_LIBRARIES})

# Copy DLL over for postbuild
add_custom_command(TARGET rswrapper POST_BUILD 
	 COMMAND ${CMAKE_COMMAND} -E copy
		 "${CMAKE_SOURCE_DIR}/external/lib/win32_realsense-d.dll"
		 "$<TARGET_FILE_DIR:rswrapper>/realsense-d.dll"
	 COMMENT "Copying DLL"
     )
ENDIF(WIN32)