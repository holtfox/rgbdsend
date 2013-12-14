#include <stdlib.h>
#include <OpenNI.h>
#include <cmath>
#include <ctime>
#include <jpeglib.h>

#include "capture.h"
#include "rgbdsend.h"
#include "config.h"

RawData::RawData(int dresx, int dresy, int cresx, int cresy) {
	this->dresx = dresx;
	this->dresy = dresy;
	
	this->cresx = cresx;
	this->cresy = cresy;
	
	r = new int[cresx*cresy];
	g = new int[cresx*cresy];
	b = new int[cresx*cresy];
	
	d = new long[dresx*dresy];
	dframenums = new int[dresx*dresy];
	
	memset(r, 0, sizeof(int)*cresx*cresy);
	memset(g, 0, sizeof(int)*cresx*cresy);
	memset(b, 0, sizeof(int)*cresx*cresy);
	
	memset(d, 0, sizeof(long)*dresx*dresy);
	memset(dframenums, 0, sizeof(int)*dresx*dresy);
		
	cframenum = 0;
}

RawData::~RawData() {
	delete[] r;
	delete[] g;
	delete[] b;
	delete[] d;
	delete[] dframenums;
}

bool init_openni_device(const char *uri, openni::Device *device, openni::VideoStream *depth, openni::VideoStream *color) {
	openni::Status rc = device->open(uri);
	if(rc != openni::STATUS_OK) {
		printf("OpenNI: Couldn't open device\n%s", openni::OpenNI::getExtendedError());
		return false;
	}
	
		
	if(device->getSensorInfo(openni::SENSOR_DEPTH) != NULL) {
		depth->create(*device, openni::SENSOR_DEPTH);
		set_maxres(*depth);				
	} else {
		printf("OpenNI: Couldn't create depth stream\n%s", openni::OpenNI::getExtendedError());		
		return false;
	}

	if(device->getSensorInfo(openni::SENSOR_COLOR) != NULL) {
		color->create(*device, openni::SENSOR_COLOR);
		set_closestres(*color, depth->getVideoMode());			
	} else {
		printf("OpenNI: Couldn't create color stream\n%s", openni::OpenNI::getExtendedError());
		return false;
	}
	
	return true;
}

static void set_cropping(openni::VideoStream *s, int r, int l, int t, int b) {
	int w = s->getVideoMode().getResolutionX();
	int h = s->getVideoMode().getResolutionY();
	int cl = w*l/100;
	int cr = w*r/100;
	int ct = h*t/100;
	int cb = h*b/100;
	
	if(s->setCropping(cl, ct, w-cl-cr, h-ct-cb) != openni::STATUS_OK) {
		printf("OpenNI Error: Invalid cropping parameters!\n");
		exit(1);
	}
}

void init_openni(openni::Device *device, openni::VideoStream *depth, openni::VideoStream *color, Config &conf) {
	openni::Status rc = openni::OpenNI::initialize();
	if(rc != openni::STATUS_OK)	{
		printf("OpenNI: Initialize failed\n%s", openni::OpenNI::getExtendedError());
		exit(1);
	}
	
	if(!init_openni_device(openni::ANY_DEVICE, device, depth, color))
		exit(1);
	
	if(device->isImageRegistrationModeSupported(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR))	
		device->setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR);
	else
		printf("OpenNI Warning: depth to image registration not supported by device!\nColor values will appear shifted.\n");
	
	set_cropping(color, conf.crop_left, conf.crop_right, conf.crop_top, conf.crop_bottom);
	set_cropping(depth, conf.crop_left, conf.crop_right, conf.crop_top, conf.crop_bottom);
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
			printf("\nONI file was read.\n");
			break;
		}
		
		streams[readyStream]->readFrame(&frame);
		read_frame(frame, raw);
// 		if(framestotake[readyStream] > 0) {
// 			streams[readyStream]->readFrame(&frame);
// 			framestotake[readyStream]--;
// 			read_frame(frame, raw);
// 			
// 			if(framestotake[readyStream] == 0) {
// 				streams[readyStream]->stop();
// 				
// 				sc--;
// 				streams[readyStream] = streams[sc];
// 				framestotake[readyStream] = framestotake[sc];
// 				printf("\n");
// 			}			
// 		}
		
// 		printf("\rframes left: ");
// 		for(int i = 0; i < sc; i++)
// 			printf("%d ", framestotake[i]);	
	}
	
	for(int i = 0; i < streamcount; i++) {
		streams[i]->stop();
	}
	printf("\n");	
}

int capture_thumbnail(unsigned char **thumbbuf, long unsigned int *memsize, openni::VideoStream &color) {
	openni::Status rc;
	int readyStream = -1;
	openni::VideoFrameRef frame;

	color.start();
	
	
	openni::VideoStream *streams[] = {&color};
	rc = openni::OpenNI::waitForAnyStream(streams, 1, &readyStream, rgbdsend::read_wait_timeout);
	if (rc != openni::STATUS_OK) {
		printf("\nRecording thumbnail timed out.\n");
		return 0;
	}
	
	color.readFrame(&frame);
	
	openni::RGB888Pixel *pixels = (openni::RGB888Pixel *)frame.getData();
	
	jpeg_compress_struct cinfo;
	jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	
	jpeg_create_compress(&cinfo);
	*thumbbuf = NULL;
	*memsize = 0;
	jpeg_mem_dest(&cinfo, thumbbuf, memsize);
	
	cinfo.image_width = frame.getWidth();
	cinfo.image_height = frame.getHeight();
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 20, true);
	jpeg_start_compress(&cinfo, true);
	
	while(cinfo.next_scanline < cinfo.image_height) {
		JSAMPROW rowpointer;
		rowpointer = (JSAMPROW) &pixels[cinfo.next_scanline*cinfo.image_width];
		jpeg_write_scanlines(&cinfo, &rowpointer, 1);
	}
	
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	color.stop();
	return 1;
}

void cleanup_openni(openni::Device &device, openni::VideoStream &depth, openni::VideoStream &color) {
	depth.destroy();
	color.destroy();
	device.close();
	openni::OpenNI::shutdown();
}
