#include <stdlib.h>
#include <OpenNI.h>
#include <cmath>
#include <ctime>

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
				int idx = x+data.dresx*y;
				
				float curavg = data.d[idx]/(float)data.dframenums[idx];
				
				if(depthpix[idx] == 0)
					continue;
				
				if(data.d[idx] == 0 || fabs(curavg-depthpix[idx]) < rgbdsend::depth_averaging_threshold) {
					data.d[idx] += depthpix[idx];
					data.dframenums[idx]++;
				}		
				
			}
		}
		break;
	case openni::PIXEL_FORMAT_RGB888:
		clrpix = (openni::RGB888Pixel*)frame.getData();
		for(y = 0; y < data.cresy; y++) {
			for(x = 0; x < data.cresx; x++) {
				int idx = x+data.cresx*y;
				
				data.r[idx] += clrpix[idx].r;
				data.g[idx] += clrpix[idx].g;
				data.b[idx] += clrpix[idx].b;			
				
			}
		}
		
		data.cframenum++;
		break;
	default:
		printf("Unknown format\n");
	}	
}

void capture(openni::VideoStream **streams, int streamcount, RawData &raw, int time) {
	openni::VideoFrameRef frame;
	openni::Status rc;
	
	int starttime = clock();
	int *framestotake = new int[streamcount];
	
	for(int i = 0; i < streamcount; i++) {
		framestotake[i] = time/CLOCKS_PER_SEC*streams[i]->getVideoMode().getFps();
		printf("take %d frames from stream %d\n", framestotake[i], i);
	}
	
	while(1) {
		int abort = 1;
		printf("\r");
		for(int i = 0; i < streamcount; i++) {
			abort = (abort && framestotake[i] == 0);
			print("%d ", framestotake[i]);
		}
		print("frames left.");
		
		if(abort)
			break;
		
		int readyStream = -1;
		rc = openni::OpenNI::waitForAnyStream(streams, streamcount, &readyStream, rgbdsend::read_wait_timeout);
		if (rc != openni::STATUS_OK) {
			printf("\nRecording timed out.\n");
			break;
		}

		if(framestotake[readyStream] > 0) {
			streams[readyStream]->readFrame(&frame);
			framestotake[readyStream]--;
			read_frame(frame, raw);
		}
	}
	
	printf("\n");
}