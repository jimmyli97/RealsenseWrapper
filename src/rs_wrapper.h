#ifndef RSWRAPPER_H
#define RSWRAPPER_H

#include <vector>
#include <map>
#include <string>
#include <exception>

#include <boost/filesystem.hpp>
#include <rs.hpp>

namespace fs = boost::filesystem;

namespace rsw {
	class RealSenseWrapper {
	public:
		enum RSError {
		};

		/// throws boost::filesystem::filesystem_error if unable to open directory
		RealSenseWrapper(std::string directory);

		~RealSenseWrapper();

		/// Returns list of serial names each corresponding to a camera
		std::vector<std::string> getDeviceList();

		void printStatus();

		/// Returns frame at specified timestamp of specified stream
		RSError getFrame(void * data, std::string serial, rs::stream strm,
			std::string streamName, int timestamp);

		RSError startStream(std::string serial, rs::stream strm, std::string streamName, rs::preset preset);

		RSError startStream(std::string serial, rs::stream strm, std::string streamName,
			int width, int height, rs::format fmt, int framerate);

		RSError stopStream(std::string serial, rs::stream strm);

	private:
		rs::device * getDevice(std::string serial);

		int i;
		rs::context ctx;
		fs::path dataPath;
		std::map<std::string, rs::device *> _deviceMap;
	};
}

#endif
