#include <stdlib.h>
#include <OpenNI.h>
#include <cmath>

#include "capture.h"
#include "rgbdsend.h"

RawData::RawData(int dresx, int dresy, int cresx, int cresy) {
	this->dresx = dresx;
	this->dresy = dresy;
	
	this->cresx = cresx;
	this->cresy = cresy;
	
	this->r = new int[cresx*cresy];
	this->g = new int[cresx*cresy];
	this->b = new int[cresx*cresy];
	
	this->d = new long[dresx*dresy];
	this->dframenums = new int[dresx*dresy];
	
	memset(this->r, 0, sizeof(int)*cresx*cresy);
	memset(this->g, 0, sizeof(int)*cresx*cresy);
	memset(this->b, 0, sizeof(int)*cresx*cresy);
	
	memset(this->d, 0, sizeof(long)*dresx*dresy);
	memset(this->dframenums, 0, sizeof(int)*dresx*dresy);
		
	this->cframenum = 0;
}

RawData::~RawData() {
	delete[] this->r;
	delete[] this->g;
	delete[] this->b;
	delete[] this->d;
	delete[] this->dframenums;
}

void init_openni(openni::Device *device) {
	openni::Status rc = openni::OpenNI::initialize();
	if(rc != openni::STATUS_OK)	{
		printf("Initialize failed\n%s\n", openni::OpenNI::getExtendedError());
		exit(1);
	}

	
	rc = device->open(openni::ANY_DEVICE);
	if(rc != openni::STATUS_OK) {
		printf("Couldn't open device\n%s\n", openni::OpenNI::getExtendedError());
		exit(2);
	}
	
	if(device->isImageRegistrationModeSupported(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR))	
		device->setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR);
	else
		printf("Warning, depth to image registration not supported by device! Color values will appear shifted.\n");
}

void set_maxres(openni::VideoStream &stream) {
	const openni::Array<openni::VideoMode> &modes = stream.getSensorInfo().getSupportedVideoModes();
	int mode = 0;
	int max = 0;
	
	for(int i = 0; i < modes.getSize(); i++) {
		int res = modes[i].getResolutionX()*modes[i].getResolutionY()+modes[i].getFps(); // higher fps slightly prefered
		if(res > max) {
			max = res;
			mode = i;
		}
	}
	
	stream.setVideoMode(modes[mode]);
}

void read_frame(openni::VideoFrameRef &frame, RawData &data) {
	openni::DepthPixel *depthpix;
	openni::RGB888Pixel *clrpix;
	int x, y;
	
	
	switch (frame.getVideoMode().getPixelFormat()) {
	case openni::PIXEL_FORMAT_DEPTH_1_MM:
	case openni::PIXEL_FORMAT_DEPTH_100_UM:
		depthpix = (openni::DepthPixel*)frame.getData();
		for(y = 0; y < data.dresy; y++) {
			for(x = 0; x < data.dresx; x++) {
				float curavg = data.d[x+data.dresx*y]/(float)data.dframenums[x+data.dresx*y];
				
				if(depthpix[x+data.dresx*y] == 0)
					continue;
				
				if(data.d[x+data.dresx*y] == 0 || fabs(curavg-depthpix[x+data.dresx*y]) < rgbdsend::depth_averaging_threshold) {
					data.d[x+data.dresx*y] += depthpix[x+data.dresx*y];
					data.dframenums[x+data.dresx*y]++;
				}		
				
			}
		}
		break;
	case openni::PIXEL_FORMAT_RGB888:
		clrpix = (openni::RGB888Pixel*)frame.getData();
		for(y = 0; y < data.cresy; y++) {
			for(x = 0; x < data.cresx; x++) {
				data.r[x+data.cresx*y] += clrpix[x+data.cresx*y].r;
				data.g[x+data.cresx*y] += clrpix[x+data.cresx*y].g;
				data.b[x+data.cresx*y] += clrpix[x+data.cresx*y].b;		
				
				
			}
		}
		
		data.cframenum++;
		break;
	default:
		printf("Unknown format\n");
	}	
}