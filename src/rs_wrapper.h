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

		/// Returns frame at specified timestamp of specified stream
		RSError getFrame(std::vector<char>** data, std::string serial, rs::stream strm,
			std::string streamName, int timestamp);

		RSError startStream(std::string serial, rs::stream strm, std::string streamName,
			int width, int height, rs::format fmt, int framerate);

		RSError stopStream(std::string serial, rs::stream strm, std::string streamName);

	private:
		rs::context ctx;
		fs::path dataPath;
		// map of serial strings to devices and mutexes. maps to NULL if disconnected
		std::map<std::string, std::pair<rs::device*, std::mutex*>> _deviceMap;
		std::shared_mutex _deviceMapM;
		// map of all paths currently being written to and their thread, timestamp, and stop status
		std::map <fs::path, std::tuple<std::thread*, int, bool>> _writeInfo;
		std::shared_mutex _writeInfoM;

		std::thread* overwatch;
		std::atomic_bool done;

		void overwatchLoop();
		void writeImgLoop(fs::path p, std::string serial, rs::stream strm,
			int width, int height, rs::format fmt, int framerate);
	};
}

#endif
