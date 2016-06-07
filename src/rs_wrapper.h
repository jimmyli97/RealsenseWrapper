#ifndef RSWRAPPER_H
#define RSWRAPPER_H

#include <vector>
#include <string>

#include <rs.hpp>

class RealSenseWrapper {
public:
	enum RSError {
	};

	RealSenseWrapper() {
		// Initialize context, initialize list of cameras
	}

	~RealSenseWrapper() {
		// Stop all streams, close all cameras, destroy context
	}

	/// Returns list of serial names each corresponding to a camera
	std::vector<std::string> getDeviceList();

	void printStatus();

	/// Returns frame at specified timestamp of specified stream
	RSError getFrame(const void * frame, std::string serial, rs::stream strm,
					 std::string streamName, int timestamp);

	RSError startStream(std::string serial, rs::stream strm, std::string streamName, rs::preset preset);

	RSError startStream(std::string serial, rs::stream strm, std::string streamName,
					 int width, int height, rs::format fmt, int framerate);

	RSError stopStream(std::string serial, rs::stream strm);

private:
	rs::device * getDevice(std::string serial);

	std::vector<rs::device *> _devices;
};

#endif
