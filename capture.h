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

void init_openni(openni::Device *device);
void set_maxres(openni::VideoStream &stream);
void read_frame(openni::VideoFrameRef &frame, RawData &data);

#endif