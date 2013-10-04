#ifndef CAPTURE_H
#define CAPTURE_H

namespace openni {
	class VideoFrameRef;
	class VideoStream;
	class Device;
};

class RawData {
public:
	RawData(int dresx, int dresy, int cresx, int cresy);
	~RawData();
	// Depth
	
	int dresx;
	int dresy;
	
	long *d;
	int *dframenums; // some pixels are rejected for the average so
					 // the number of frames is pixel dependent
	
	// Color
	
	int cresx; 
	int cresy; 
	
	int *r;
	int *g;
	int *b;
	int cframenum;	
};

void init_openni(openni::Device *device, openni::VideoStream *depth, openni::VideoStream *color);
void set_maxres(openni::VideoStream &stream);
void set_closestres(openni::VideoStream &stream, const openni::VideoMode &target);
void read_frame(openni::VideoFrameRef &frame, RawData &data);
void capture(openni::VideoStream **streams, int streamcount, RawData &data, int *framecounts);

void cleanup_openni(openni::Device &device, openni::VideoStream &depth, openni::VideoStream &color);

#endif