#include <cstdio>
#include <ctime>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <OpenNI.h>

#include "rgbdsend.h"
#include "capture.h"
#include "pointcloud.h"
#include "sendfile.h"
#include "config.h"


int main(int argc, char **argv) {
	openni::Device device;	
	openni::Status rc;
			
	char *prefix = strrchr(argv[0], '/')+1;
	char *cfgfile = new char[prefix-argv[0]+sizeof(rgbdsend::config_file_name)+1];
	
	strncpy(cfgfile, argv[0], prefix-argv[0]+1);
	strcpy(cfgfile+(prefix-argv[0]), rgbdsend::config_file_name);
		
	Config conf;
	if(conf.read(cfgfile) != 1) {
		printf("Config: Falling back to builtin presets.\n");
		conf.setDefaults();
	}
		
	delete[] cfgfile;
		
	init_openni(&device);
	CURL *curl = init_curl();
	
	openni::VideoStream depth, color;
		
	if(device.getSensorInfo(openni::SENSOR_DEPTH) != NULL) {
		depth.create(device, openni::SENSOR_DEPTH);
		set_maxres(depth);				
	} else {
		printf("OpenNI: Couldn't create depth stream\n%s", openni::OpenNI::getExtendedError());		
		exit(1);
	}

	if(device.getSensorInfo(openni::SENSOR_COLOR) != NULL) {
		color.create(device, openni::SENSOR_COLOR);
		set_closestres(color, depth.getVideoMode());			
	} else {
		printf("OpenNI: Couldn't create color stream\n%s", openni::OpenNI::getExtendedError());
		exit(1);
	}

	printf("Resolution:\nDepth: %dx%d @ %d fps\nColor: %dx%d @ %d fps\n",
		   depth.getVideoMode().getResolutionX(), depth.getVideoMode().getResolutionY(), depth.getVideoMode().getFps(),
		   color.getVideoMode().getResolutionX(), color.getVideoMode().getResolutionY(), color.getVideoMode().getFps());
	
	char tmpfile[256];	
	
	while(1) {		
		printf("Press Enter to record and send a segment (q to quit): ");
		if(getc(stdin) == 'q')
			break;
		
		time_t t = time(NULL);
		strftime(tmpfile, sizeof(tmpfile), "rgbd_%Y%m%d_%H-%M-%S.ply", localtime(&t));
		
		RawData raw(depth.getVideoMode().getResolutionX(), depth.getVideoMode().getResolutionY(),
					color.getVideoMode().getResolutionX(), color.getVideoMode().getResolutionY());
				
		printf("Recording started.\n");
		
		openni::VideoStream* streams[] = {&depth, &color};
		int framecounts[] = {conf.capture_depth_frames, conf.capture_color_frames};
		capture(streams, 2, raw, framecounts);
			
		printf("Recording ended.\n");
		
		
		PointCloud cloud(raw.dresx*raw.dresy);
		depth_to_pointcloud(cloud, raw, depth, color);
		export_to_ply(tmpfile, cloud);
		
		printf("\nExtracted to point cloud: %s\n", tmpfile);
		
		if(conf.dest_url && conf.dest_username && conf.dest_password)
			send_file(curl, tmpfile, conf.dest_url, conf.dest_username, conf.dest_password);
		else
			printf("No destination server specified. Skipping transfer.\n");
	}
	
	
	depth.destroy();
	color.destroy();
	device.close();
	openni::OpenNI::shutdown();
}