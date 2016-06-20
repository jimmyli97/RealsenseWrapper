#ifndef RSWRAPPER_H
#define RSWRAPPER_H

#include <vector>
#include <tuple>
#include <map>
#include <string>
#include <exception>
#include <thread>
#include <shared_mutex>
#include <atomic>
#include <utility>

#include <boost/filesystem.hpp>
#include <rs.hpp>

namespace fs = boost::filesystem;

namespace rsw {
	class RealSenseWrapper {
	public:
		enum RSError {
			NO_ERROR = 0,
			UNABLE_TO_ACCESS
		};

		/// throws boost::filesystem::filesystem_error if unable to open directory
		/// throws std::invalid_argument if directory does not exist
		RealSenseWrapper(std::string directory);

		~RealSenseWrapper();

		/// Returns list of serial names each corresponding to a camera
		std::vector<std::string>* getDeviceList();

		void printStatus();

		/// Returns frame at specified timestamp of specified stream, or the latest timestamp if
		/// none specified.
		RSError getFrame(std::vector<char>** data, std::string serial, rs::stream strm,
			std::string streamName, int timestamp = -1);

		RSError enableStream(std::string serial, rs::stream strm, std::string streamName,
			int width, int height, rs::format fmt, int framerate);

		RSError disableStream(std::string serial, rs::stream strm, std::string streamName);

		RSError startDevice(std::string serial);
		
		RSError stopDevice(std::string serial);

	private:
		rs::context ctx;
		fs::path dataPath;
		// map of serial strings to devices and mutexes and
		// a map of stream types to thread pointers, last written timestamp, and whether to stop or not.
		// maps to NULL tuple if device is disconnected
		std::map<std::string,
			std::tuple<rs::device*, std::mutex*,
				std::map<rs::stream, std::tuple<std::thread*, int, bool>>>> _deviceMap;
		std::shared_mutex _deviceMapM;

		void overwatchLoop();
		void writeImgLoop(fs::path p, std::string serial, rs::stream strm,
			int width, int height, rs::format fmt, int framerate);
	};
}

#endif
