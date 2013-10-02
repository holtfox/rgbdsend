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
		printf("OpenNI: Initialize failed\n%s", openni::OpenNI::getExtendedError());
		exit(1);
	}

	
	rc = device->open(openni::ANY_DEVICE);
	if(rc != openni::STATUS_OK) {
		printf("OpenNI: Couldn't open device\n%s", openni::OpenNI::getExtendedError());
		exit(2);
	}
	
	if(device->isImageRegistrationModeSupported(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR))	
		device->setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR);
	else
		printf("OpenNI Warning: depth to image registration not supported by device!\nColor values will appear shifted.\n");
}

void set_maxres(openni::VideoStream &stream) {
	const openni::Array<openni::VideoMode> &modes = stream.getSensorInfo().getSupportedVideoModes();
	int mode = 0;
	int max = 0;
	
	for(int i = 0; i < modes.getSize(); i++) {
		int res = modes[i].getResolutionX()*modes[i].getResolutionY()+modes[i].getFps(); // lower fps slightly prefered
		if(res > max && (modes[i].getPixelFormat() == openni::PIXEL_FORMAT_DEPTH_100_UM // we don't want yuv422.
			|| modes[i].getPixelFormat() == openni::PIXEL_FORMAT_DEPTH_1_MM
			|| modes[i].getPixelFormat() == openni::PIXEL_FORMAT_RGB888)) {
			max = res;
			mode = i;
		}
	}
	
	stream.setVideoMode(modes[mode]);
}

void set_closestres(openni::VideoStream &stream, const openni::VideoMode &target) {
	const openni::Array<openni::VideoMode> &modes = stream.getSensorInfo().getSupportedVideoModes();
	int mode = 0;
	int min = 1<<16;
	
	for(int i = 0; i < modes.getSize(); i++) {
		int dx = target.getResolutionX()-modes[i].getResolutionX();
		dx *= dx;
		int dy = target.getResolutionY()-modes[i].getResolutionY();
		dy *= dy;
		
		int res = dx+dy+modes[i].getFps(); // higher fps slightly prefered
		if(res < min && (modes[i].getPixelFormat() == openni::PIXEL_FORMAT_DEPTH_100_UM // we don't want yuv422.
			|| modes[i].getPixelFormat() == openni::PIXEL_FORMAT_DEPTH_1_MM
			|| modes[i].getPixelFormat() == openni::PIXEL_FORMAT_RGB888)) {
			min = res;
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

void capture(openni::VideoStream **streams, int streamcount, RawData &raw, int *framecounts) {
	openni::VideoFrameRef frame;
	openni::Status rc;
	
	int *framestotake = new int[streamcount];
	
	for(int i = 0; i < streamcount; i++) {
		streams[i]->start();		
		framestotake[i] = framecounts[i];
		printf("take %d frames from stream %d\n", framestotake[i], i);
	}
	
	int sc = streamcount;
	
	while(sc) {
		int readyStream = -1;
		rc = openni::OpenNI::waitForAnyStream(streams, sc, &readyStream, rgbdsend::read_wait_timeout);
		if (rc != openni::STATUS_OK) {
			printf("\nRecording timed out.\n");
			break;
		}

		if(framestotake[readyStream] > 0) {
			streams[readyStream]->readFrame(&frame);
			framestotake[readyStream]--;
			read_frame(frame, raw);
			
			if(framestotake[readyStream] == 0) {
				streams[readyStream]->stop();
				
				sc--;
				streams[readyStream] = streams[sc];
				framestotake[readyStream] = framestotake[sc];
				printf("\n");
			}			
		}
		
		printf("\rframes left: ");
		for(int i = 0; i < sc; i++)
			printf("%d ", framestotake[i]);	
	}
	printf("\n");	
}
