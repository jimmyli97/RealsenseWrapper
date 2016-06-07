#include <iostream>
#include <string>
#include <rs.hpp>

#include "rs_wrapper.h"

namespace rsw {
	RealSenseWrapper::RealSenseWrapper(std::string directory) :
									   ctx(), _deviceMap(), dataPath(directory) {
		rs::log_to_console(rs::log_severity::warn);
		
		// Open directory, check validity
		if (fs::exists(dataPath) && fs::is_directory(dataPath)) {
			// Initialize mapping
			for (auto dirEnt : fs::directory_iterator(dataPath)) {
				std::string fn = dirEnt.path().filename().string();
				std::cout << "Found recordings for device: " << fn << std::endl;
				_deviceMap[fn] = NULL;
			}

			fs::create_directory(dataPath / "1234");

			// Find matching connected devices, add to map
			for (int i = 0; i < ctx.get_device_count(); ++i) {
				std::string serial = std::string(ctx.get_device(i)->get_serial());
				std::cout << "Found connected device: " << serial << std::endl;
				
				if (_deviceMap.count(serial) == 0) {
					// No recordings exist, create a folder
					fs::create_directory(dataPath / serial);
				}
				_deviceMap[serial] = ctx.get_device(i);
			}
		}
		else {
			throw new std::invalid_argument("Invalid argument: Unable to open folder");
		}
	}

	RealSenseWrapper::~RealSenseWrapper() {
		// Stop all streams, close all cameras, destroy context
	}
}

using namespace std;

int main(void) try {
	std::string dirName = "C:\\Users\\yimmy\\Documents\\work\\CAMP2016\\rswrapper_frames";
	rsw::RealSenseWrapper realsense(dirName);

	// TEST: get list of devices
	std::vector<std::string> deviceList = realsense.getDeviceList();
	for (auto str : deviceList) {
		std::cout << "Device Serial: " << str << std::endl;
	}

	// TEST: print status of devices (attached/not attached, available playback, if stream is streaming)
	realsense.printStatus();

	// TEST: frame does not exist error
	void * data;
	int ret = realsense.getFrame(data, "serial", rs::stream::color, "serial_color", 102);
	// TODO: check error code here

	// TEST: starting multiple streams
	ret = realsense.startStream("serial", rs::stream::depth, "serial_depth", rs::preset::highest_framerate);
	ret = realsense.startStream("serial", rs::stream::color, "serial_color", rs::preset::highest_framerate);

	// TEST: stopping only one stream
	ret = realsense.stopStream("serial", rs::stream::color);

	// TEST: grabbing frame data from stopped stream and from live stream
	ret = realsense.getFrame(data, "serial", rs::stream::color, "serial_color", 203);
	ret = realsense.getFrame(data, "serial", rs::stream::depth, "serial_depth", 204);

	// TODO: test stream name conflicts
	std::cout << "Waiting...";
	std::string i;
	std::cin >> i;
	return 0;
}
catch (const rs::error & e)
{
	std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
	return EXIT_FAILURE;
}
catch (const std::exception & e) {
	std::cout << e.what() << std::endl;
	return EXIT_FAILURE;
}
