#ifndef RSWRAPPER_H
#define RSWRAPPER_H

#include <vector>
#include <tuple>
#include <map>
#include <string>
#include <exception>
#include <thread>
#include <mutex>
#include <atomic>

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

		/// Returns frame at specified timestamp of specified stream
		RSError getFrame(std::vector<char>** data, std::string serial, rs::stream strm,
			std::string streamName, int timestamp);

		RSError startStream(std::string serial, rs::stream strm, std::string streamName,
			int width, int height, rs::format fmt, int framerate);

		RSError stopStream(std::string serial, rs::stream strm);

	private:
		int i;
		rs::context ctx;
		fs::path dataPath;
		// map of serial strings to devices and mutexes. device is NULL if it's disconnected
		std::map<std::string, pair<rs::device*, std::mutex*>> _deviceMap;
		// map of all paths currently being written to and their corresponding timestamp and thread
		std::map <fs::path, pair<std::thread*, std::atomic_int>> _writeInfo;
	};
}

#endif
