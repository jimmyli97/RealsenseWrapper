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
