#include <iostream>
#include <string>
#include <ios>
#include <tuple>
#include <thread>

#include "boost/filesystem/fstream.hpp"
#include "rs_wrapper.h"
#include "rs.hpp"
#include "glfw3.h"

#define TEST_SERIAL "023150187408"

namespace rsw {
	RealSenseWrapper::RealSenseWrapper(std::string directory) :
									   ctx(), _deviceMap(), _deviceMapM(), 
									   dataPath(directory) {
		rs::log_to_console(rs::log_severity::debug);
		
		// Open directory, check validity
		if (fs::exists(dataPath) && fs::is_directory(dataPath)) {
			// Initialize mapping
			for (auto dirEnt : fs::directory_iterator(dataPath)) {
				std::string fn = dirEnt.path().filename().string();
				std::cout << "Found recordings for device: " << fn << std::endl;
				_deviceMap[fn] = std::pair<rs::device*, std::mutex*>(nullptr, nullptr);
			}

			// Find matching connected devices, add to map
			for (int i = 0; i < ctx.get_device_count(); ++i) {
				std::string serial = std::string(ctx.get_device(i)->get_serial());
				std::cout << "Found connected device: " << serial << std::endl;
				
				if (_deviceMap.count(serial) == 0) {
					// No recordings exist, create a folder
					fs::create_directory(dataPath / serial);
				}
				_deviceMap[serial] = std::make_tuple(ctx.get_device(i), new std::mutex(),
					std::get<2>(_deviceMap[serial]));
			}
		}
		else {
			throw new std::invalid_argument("Invalid argument: Unable to open folder");
		}
	}

	RealSenseWrapper::~RealSenseWrapper() {
		// Stop all threads
		for (auto dev : _deviceMap) {
			for (auto threads : std::get<2>(dev.second)) {
			}
		}
		// Release all mutexes
	}

	std::vector<std::string>* RealSenseWrapper::getDeviceList() {
		std::vector<std::string>* out = new std::vector<std::string>();
		_deviceMapM.lock_shared();
		for (auto items : _deviceMap) {
			out->push_back(items.first);
		}
		_deviceMapM.unlock_shared();
		return out;
	}

	void RealSenseWrapper::printStatus() {
		_deviceMapM.lock_shared();
		for (auto items : _deviceMap) {
			rs::device* dev = items.second.first;
			std::cout << items.first << ": " << std::endl;
			if (dev == nullptr) {
				std::cout << "  Connected: No" << std::endl;
			} else {
				items.second.second->lock();
				std::cout << "  Connected: Yes" << std::endl;
				std::cout << "  Device: " << dev->get_name() << std::endl;
				/*
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
				*/
				items.second.second->unlock();
			}

			std::cout << "  Available Playback:" << std::endl;
			for (auto stream : fs::directory_iterator(dataPath / items.first)) {
				std::cout << "    " << stream.path().stem() << std::endl;
				for (auto strmName : fs::directory_iterator(stream.path())) {
					std::cout << "      " << strmName.path().stem() << std::endl;
				}
			}
		}
		_deviceMapM.unlock_shared();
	}

	RealSenseWrapper::RSError RealSenseWrapper::getFrame(std::vector<char>** data,
		std::string serial, rs::stream strm, std::string streamName, int timestamp) {

		fs::path p = dataPath / serial / rs_stream_to_string((rs_stream)strm) / streamName;

		// Check if timestamp is available/if latest frame is available
		_writeInfoM.lock_shared();
		auto writingInfo = _writeInfo.find(p);
		bool accessible = false;
		if (writingInfo == _writeInfo.end() || (timestamp < std::get<1>(writingInfo->second))) {
			accessible = true;
		} else if (timestamp == -1) {
			// get latest accessible timestamp
			timestamp = std::get<1>(writingInfo->second);
			while (--timestamp >= 0) {
				if (fs::exists(p / std::to_string(timestamp))) {
					accessible = true;
					break;
				}
			}
		}
		_writeInfoM.unlock_shared();

		if (accessible) {
			// Read path
			if (fs::is_regular_file(p / std::to_string(timestamp))) {
				fs::ifstream ifs(p / std::to_string(timestamp), std::ios::in | std::ios::binary);
				std::streamsize size = ifs.tellg();
				ifs.seekg(0, std::ios::beg);
				auto readData = new std::vector<char>(size);
				if (ifs.read(readData->data(), size)) {
					*data = readData;
					return NO_ERROR;
				}
			}
		}
		return UNABLE_TO_ACCESS;
	}

	// Copied from librealsense/src/image.cpp
	int getImgSize(int width, int height, rs_format format) {
		switch (format)
		{
		case RS_FORMAT_Z16: return width * height * 2;
		case RS_FORMAT_DISPARITY16: return width * height * 2;
		case RS_FORMAT_XYZ32F: return width * height * 12;
		case RS_FORMAT_YUYV: assert(width % 2 == 0); return width * height * 2;
		case RS_FORMAT_RGB8: return width * height * 3;
		case RS_FORMAT_BGR8: return width * height * 3;
		case RS_FORMAT_RGBA8: return width * height * 4;
		case RS_FORMAT_BGRA8: return width * height * 4;
		case RS_FORMAT_Y8: return width * height;
		case RS_FORMAT_Y16: return width * height * 2;
		case RS_FORMAT_RAW10: assert(width % 4 == 0); return width * 5 / 4 * height;
		default: assert(false); return 0;
		}
	}

	void RealSenseWrapper::writeImgLoop(fs::path p, std::string serial, rs::stream strm,
			int width, int height, rs::format fmt, int framerate) {
		_deviceMapM.lock_shared();
		_deviceMap[serial].second->lock();
		auto dev = _deviceMap[serial].first;
		if (dev->is_streaming()) {
			dev->stop();
		}
		dev->enable_stream(strm, width, height, fmt, framerate);
		dev->start();
		_deviceMap[serial].second->unlock();
		_deviceMapM.unlock_shared();

		int imgSize = getImgSize(width, height, (rs_format)fmt);

		while (true) {
			_writeInfoM.lock_shared();
			// check if we should stop
			if (std::get<2>(_writeInfo[p]) == true) {
				break;
			} else {
				_deviceMapM.lock_shared();
				_deviceMap[serial].second->lock();
				rs::device* dev = _deviceMap[serial].first;
				if (dev->poll_for_frames()) {
					int timestamp = dev->get_frame_timestamp(strm);
					const char* data = static_cast<const char*>(dev->get_frame_data(strm));
					fs::ofstream ofs(p / std::to_string(timestamp));

					std::cout << "Writing image";

					// write to file
					ofs.write(data, imgSize);

					_writeInfo[p] = std::make_tuple(std::get<0>(_writeInfo[p]), timestamp, false);
				}
				_deviceMap[serial].second->unlock();
				_deviceMapM.unlock_shared();
			}
			_writeInfoM.unlock_shared();
			std::this_thread::yield();
		}
	}

	RealSenseWrapper::RSError RealSenseWrapper::enableStream(std::string serial,
			rs::stream strm, std::string streamName, int width, int height,
			rs::format fmt, int framerate) {
		fs::path p(dataPath / serial / rs_stream_to_string((rs_stream)strm) / streamName);
		_deviceMapM.lock_shared();
		_writeInfoM.lock_shared();
		// Check if we're writing to this stream already or if the name already exists
		if ((_writeInfo.find(p) == _writeInfo.end()) && !fs::exists(p) && 
				_deviceMap[serial].first != nullptr) {
			_writeInfoM.unlock_shared();
			// Create folders
			fs::create_directories(p);

			// Start thread 
			auto writeThread = new std::thread(&RealSenseWrapper::writeImgLoop, this,
				p, serial, strm, width, height, fmt, framerate);

			_writeInfoM.lock();
			_writeInfo[p] = std::make_tuple(writeThread, 0, false);
			_writeInfoM.unlock();

			_deviceMapM.unlock_shared();
			return NO_ERROR;
		} else {
			_writeInfoM.unlock_shared();
			return UNABLE_TO_ACCESS;
		}
	}

	RealSenseWrapper::RSError RealSenseWrapper::stopDevice(std::string serial) {
		_deviceMapM.lock_shared();
		if (_deviceMap[serial].first != nullptr) {
			_writeInfoM.lock();
			if (_writeInfo.find(p) != _writeInfo.end()) {
				_writeInfo[p] = std::make_tuple(std::get<0>(_writeInfo[p]),
					std::get<1>(_writeInfo[p]), true);
			}
			_writeInfoM.unlock();
		}
		_deviceMapM.unlock_shared();
		return NO_ERROR;
	}
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

	vector<char>** depthData = new vector<char>*;
	vector<char>** colorData = new vector<char>*;
	int ret;

	// TEST: starting multiple streams
	ret = realsense.enableStream(TEST_SERIAL, rs::stream::color, "serial_color", 640, 480, rs::format::rgb8, 30);
	ret = realsense.enableStream(TEST_SERIAL, rs::stream::depth, "serial_depth", 640, 480, rs::format::z16, 30);

	ret = realsense.startDevice(TEST_SERIAL);

	// TEST: stopping all streams
	ret = realsense.stopDevice(TEST_SERIAL);

	// TEST: grabbing frame data from stopped stream and from live stream
	glfwInit();
	GLFWwindow * win = glfwCreateWindow(1300, 1000, "depth and color", nullptr, nullptr);
	while (true) {
		ret = realsense.getFrame(colorData, TEST_SERIAL, rs::stream::color, "serial_color");
		ret = realsense.getFrame(depthData, TEST_SERIAL, rs::stream::depth, "serial_depth");
		ret = 1;
		glfwPollEvents();
		if (ret == rsw::RealSenseWrapper::RSError::NO_ERROR) {
			glfwMakeContextCurrent(win);
			glClear(GL_COLOR_BUFFER_BIT);
			glPixelZoom(1, -1);
			glRasterPos2f(0, 1);
			glDrawPixels(640, 480, GL_RGB, GL_UNSIGNED_BYTE, &colorData[0]);
			//glRasterPos2f(640, 1);
			//glDrawPixels(640, 480, GL_RGB, GL_UNSIGNED_BYTE, &depthData[0]);
			glfwSwapBuffers(win);
			delete *colorData;
			delete *depthData;
		}
	}

	delete colorData;
	delete depthData;
	glfwTerminate();

	ret = realsense.stopDevice(TEST_SERIAL);

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
