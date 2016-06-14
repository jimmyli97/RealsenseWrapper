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
	void RealSenseWrapper::overwatchLoop() {
		while (!done) {
			_writeInfoM.lock();
			std::vector<fs::path> toDelete;
			for (auto info : _writeInfo) {
				if (std::get<2>(info.second)) {
					std::get<0>(info.second)->join();
					toDelete.push_back(info.first);
				}
			}
			for (auto del : toDelete) {
				delete std::get<0>(_writeInfo[del]);
				_writeInfo.erase(del);
			}
			_writeInfoM.unlock();
			std::this_thread::yield();
		}
		// stop all threads
		_writeInfoM.lock();
		for (auto info : _writeInfo) {
			info.second = make_tuple(std::get<0>(info.second), std::get<1>(info.second), true);
			std::get<0>(info.second)->join();
			delete std::get<0>(info.second);
		}
		_writeInfoM.unlock();
	}
	
	RealSenseWrapper::RealSenseWrapper(std::string directory) :
									   ctx(), _deviceMap(), _deviceMapM(), 
									   _writeInfo(), _writeInfoM(), dataPath(directory),
									   done(false), overwatch() {
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
				_deviceMap[serial].first = ctx.get_device(i);
				_deviceMap[serial].second = new std::mutex();
			}
			overwatch = new std::thread(&RealSenseWrapper::overwatchLoop, this);
		}
		else {
			throw new std::invalid_argument("Invalid argument: Unable to open folder");
		}
	}

	RealSenseWrapper::~RealSenseWrapper() {
		// Stop all threads
		done = true;
		overwatch->join();
		delete overwatch;
		// Release all mutexes
		for (auto item : _deviceMap) {
			delete item.second.second;
		}
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
				std::cout << "  Supported Streams: " << std::endl;

				/*
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
		fs::path p = dataPath / serial / rs_stream_to_string((rs_stream) strm) / streamName;
		// Check if path is currently being written to
		_writeInfoM.lock_shared();
		auto writingInfo = _writeInfo.find(p);
		if (writingInfo == _writeInfo.end() || (timestamp < std::get<1>(writingInfo->second))) {
			_writeInfoM.unlock_shared();
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

	void RealSenseWrapper::writeImgLoop(fs::path p, std::string serial, rs::stream strm,
			int width, int height, rs::format fmt, int framerate) {
		_deviceMapM.lock_shared();
		_deviceMap[serial].second->lock();
		//_deviceMap[serial].first->enable_stream(strm, width, height, fmt, framerate);
		_deviceMap[serial].first->enable_stream(strm, rs::preset::highest_framerate);
		_deviceMap[serial].first->start();
		_deviceMap[serial].second->unlock();
		_deviceMapM.unlock_shared();

		/* GLFW DEBUGGING */
		GLFWwindow * win = glfwCreateWindow(1280, 960, "color", nullptr, nullptr);
		
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

					// not thread safe
					glfwMakeContextCurrent(win);
					glfwPollEvents();
					glClear(GL_COLOR_BUFFER_BIT);
					glPixelZoom(1, -1);
					glRasterPos2f(0, 1);
					glDrawPixels(640, 480, GL_RGB, GL_UNSIGNED_BYTE, data);
					glfwSwapBuffers(win);

					// write to file
					ofs.write(data, 640 * 480);

					_writeInfo[p] = std::make_tuple(std::get<0>(_writeInfo[p]), timestamp, false);
				}
				_deviceMap[serial].second->unlock();
				_deviceMapM.unlock_shared();
			}
			_writeInfoM.unlock_shared();
			std::this_thread::yield();
		}
	}

	RealSenseWrapper::RSError RealSenseWrapper::startStream(std::string serial,
			rs::stream strm, std::string streamName, int width, int height,
			rs::format fmt, int framerate) {
		fs::path p(dataPath / serial / rs_stream_to_string((rs_stream)strm) / streamName);
		_deviceMapM.lock_shared();
		_writeInfoM.lock_shared();
		// Check if we're writing to this stream already or if the name already exists
		if (_writeInfo.find(p) == _writeInfo.end() && !fs::exists(p) && 
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

	RealSenseWrapper::RSError RealSenseWrapper::stopStream(std::string serial, rs::stream strm,
			std::string streamName) {
		_deviceMapM.lock_shared();
		if (_deviceMap[serial].first != nullptr) {
			fs::path p(dataPath / serial / rs_stream_to_string((rs_stream)strm) / streamName);
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

	vector<char>* init = 0;
	vector<char>** data = &init;
	int ret;

	/*
	// TEST: frame does not exist error
	int ret = realsense.getFrame(data, "serial", rs::stream::color, "serial_color", 102);
	// TODO: check error code here
	*/

	glfwInit();

	// TODO: get GLFW working with multiple threads, test playback by reading frames

	// TEST: starting multiple streams
	ret = realsense.startStream(TEST_SERIAL, rs::stream::color, "serial_color", 640, 480, rs::format::rgb8, 30);
	// we have to enable all streams, then start device, or start/stop device again ret = realsense.startStream(TEST_SERIAL, rs::stream::color, "serial_color", 640, 480, rs::format::any, 30);

	// TEST: stopping only one stream
	//ret = realsense.stopStream(TEST_SERIAL, rs::stream::color, "serial_color");

	// TEST: grabbing frame data from stopped stream and from live stream
	ret = realsense.getFrame(data, TEST_SERIAL, rs::stream::color, "serial_color", 3751);
	// TODO FIX THIS GLFW CODE
	GLFWwindow * win = glfwCreateWindow(1280, 960, "color", nullptr, nullptr);
	glfwMakeContextCurrent(win);
	glfwPollEvents();
	glClear(GL_COLOR_BUFFER_BIT);
	glPixelZoom(1, -1);
	glRasterPos2f(0, 1);
	glDrawPixels(640, 480, GL_RGB, GL_UNSIGNED_BYTE, &data[0]);
	glfwSwapBuffers(win);

	// TODO: test grabbing frame data from live stream

	//ret = realsense.stopStream("serial", rs::stream::depth, "serial_depth");

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
