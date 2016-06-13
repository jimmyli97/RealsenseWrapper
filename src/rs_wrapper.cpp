#include <iostream>
#include <string>
#include <ios>

#include "boost/filesystem/fstream.hpp"
#include "rs_wrapper.h"
#include "rs.hpp"

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

			// Find matching connected devices, add to map
			for (int i = 0; i < ctx.get_device_count(); ++i) {
				std::string serial = std::string(ctx.get_device(i)->get_serial());
				std::cout << "Found connected device: " << serial << std::endl;
				
				if (_deviceMap.count(serial) == 0) {
					// No recordings exist, create a folder
					fs::create_directory(dataPath / serial);
				}
				_deviceMap[serial].first = ctx.get_device(i);
			}
		}
		else {
			throw new std::invalid_argument("Invalid argument: Unable to open folder");
		}
	}

	RealSenseWrapper::~RealSenseWrapper() {
		// Join all threads, release all mutexes, stop all devices
	}

	std::vector<std::string>* RealSenseWrapper::getDeviceList() {
		std::vector<std::string>* out = new std::vector<std::string>();
		for (auto items : _deviceMap) {
			out->push_back(items.first);
		}
		return out;
	}

	void RealSenseWrapper::printStatus() {
		for (auto items : _deviceMap) {
			rs::device* dev = items.second.first;
			std::cout << items.first << ": " << std::endl;
			if (dev == NULL) {
				std::cout << "  Connected: No" << std::endl;
			} else {
				std::cout << "  Connected: Yes" << std::endl;
				std::cout << "  Device: " << dev->get_name() << std::endl;
				std::cout << "  Supported Streams: " << std::endl;

				// Show which streams are supported by this device
				for (int j = 0; j < rs_stream::RS_STREAM_COUNT; ++j) {
					// Determine number of available streaming modes (zero means stream is unavailable) 
					rs::stream strm = (rs::stream)j;
					int mode_count = dev->get_stream_mode_count(strm);
					if (mode_count == 0) continue;

					// Show each available mode for this stream
					std::cout << "    Stream " << strm << " - " << mode_count << " modes:\n";
					for (int k = 0; k < mode_count; ++k) {
						// Show width, height, format, and framerate, the settings required to enable the stream in this mode
						int width, height, framerate;
						rs::format format;
						dev->get_stream_mode(strm, k, width, height, format, framerate);
						std::cout << "      " << width << "\tx " << height << "\t@ " << framerate << "Hz\t" << format << std::endl;
					}
				}
			}
			std::cout << "  Available Playback:" << std::endl;
			for (auto stream : fs::directory_iterator(dataPath / items.first)) {
				std::cout << "    " << stream.path().stem() << std::endl;
				for (auto strmName : fs::directory_iterator(stream.path())) {
					std::cout << "      " << strmName.path().stem() << std::endl;
				}
			}
		}
	}

	RealSenseWrapper::RSError RealSenseWrapper::getFrame(std::vector<char>** data,
			std::string serial, rs::stream strm, std::string streamName, int timestamp) {
		fs::path p = dataPath / serial / rs_stream_to_string((rs_stream) strm) / streamName;
		// Check if timestamp exists/is currently being written to
		auto writingInfo = _writeInfo.find(p);
		if (writingInfo == _writeInfo.end() || (timestamp < writingInfo->second->second)) {
			// Read path
			fs::ifstream ifs(p / std::to_string(timestamp), std::ios::in | std::ios::binary);
			std::streamsize size = ifs.tellg();
			ifs.seekg(0, std::ios::beg);
			auto readData = new std::vector<char>(size);
			if (ifs.read(readData->data(), size)) {
				*data = readData;
				return NO_ERROR;
			}
		}
		return UNABLE_TO_ACCESS;
	}

	void writeImg(fs::path p, std::string serial, rs::stream strm) {
		// acquire mutex on device
		// poll for images
		// write to path if images available with corresponding timestamp
		// update timestamp in map
		// release mutex
	}

	RealSenseWrapper::RSError RealSenseWrapper::startStream(std::string serial,
			rs::stream strm, std::string streamName, int width, int height,
			rs::format fmt, int framerate) {
		fs::path p(dataPath / serial / rs_stream_to_string((rs_stream)strm) / streamName);
		// Check if we're writing to this stream already or if the name already exists
		if (_writeInfo.find(p) == _writeInfo.end() && !fs::exists(p)) {
			_deviceMap[serial].first->enable_stream(strm, width, height, fmt, framerate);
			_deviceMap[serial].first->start;

			// Create folders
			fs::create_directories(p);

			// Start thread 
			auto writeThread = new std::thread(writeImg, p, serial, strm);
			return NO_ERROR;
		} else {
			return UNABLE_TO_ACCESS;
		}
	}

	RealSenseWrapper::RSError RealSenseWrapper::stopStream(std::string serial, rs::stream strm) {
		return NO_ERROR;
	}
}

using namespace std;

int main(void) try {
	std::string dirName = "C:\\Users\\yimmy\\Documents\\work\\CAMP2016\\rswrapper_frames";
	rsw::RealSenseWrapper realsense(dirName);

	// TEST: get list of devices
	std::vector<std::string>* deviceList = realsense.getDeviceList();
	for (auto str : *deviceList) {
		std::cout << "Device Serial: " << str << std::endl;
	}
	delete deviceList;

	// TEST: print status of devices (attached/not attached, available playback, if stream is streaming)
	realsense.printStatus();

	/*
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
	*/
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
