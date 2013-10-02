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


struct Config {
	char *url;
	
	char *username;
	char *password;
	
	int capture_time; // in clocks
};

void read_config(char *filename, Config *conf) {
	char buf[512];
	int line = 0;
	int buflen;
	
	FILE *cfgfile = fopen(filename, "r");
	if(cfgfile == NULL) {
		printf("Config Error: '%s': %s.\n", filename, strerror(errno));
		exit(3);
	}
		
	while(fgets(buf, sizeof(buf), cfgfile)) {
		if(buf[0] == '#' || buf[0] == '\n')
			continue;
		
		buflen = strlen(buf);
		
		switch(line) {
		case 0: // URL
			conf->url = new char[buflen];
			strncpy(conf->url, buf, buflen);
			conf->url[buflen-1] = '\0';
			break;
		case 1: // Username
			conf->username = new char[buflen];
			strncpy(conf->username, buf, buflen);
			conf->username[buflen-1] = '\0';
			break;
		case 2: // Password
			conf->password = new char[buflen];
			strncpy(conf->password, buf, buflen);
			conf->password[buflen-1] = '\0';
			break;
		case 3: // Record 
			conf->capture_time = CLOCKS_PER_SEC*atoi(buf)/1000;
			break;
		}
		
		line++;
	}
	
	if(line != 4) {
		printf("Config Error: Wrong config file format in '%s'. Must fit the following:"
			"\n\nTargetURL\nUsername\nPassword\nCaptureTime [ms]\n\n(lines beginning with # are ignored)\n", filename);
		exit(3);
	}
		
	fclose(cfgfile);
	
	printf("Successfully read '%s'\n", filename);
}

int main(int argc, char **argv) {
	openni::Device device;	
	openni::Status rc;
			
	char *prefix = strrchr(argv[0], '/')+1;
	char *cfgfile = new char[prefix-argv[0]+sizeof(rgbdsend::config_file_name)+1];
	
	strncpy(cfgfile, argv[0], prefix-argv[0]+1);
	strcpy(cfgfile+(prefix-argv[0]), rgbdsend::config_file_name);
		
	Config conf;
	
	read_config(cfgfile, &conf);
	
	delete[] cfgfile;
		
	init_openni(&device);
	CURL *curl = init_curl();
	
	openni::VideoStream depth, color;
		
	if(device.getSensorInfo(openni::SENSOR_DEPTH) != NULL) {
		depth.create(device, openni::SENSOR_DEPTH);
		set_maxres(depth);				
	} else {
		printf("Couldn't create depth stream\n%s\n", openni::OpenNI::getExtendedError());		
		exit(1);
	}

	if(device.getSensorInfo(openni::SENSOR_COLOR) != NULL) {
		color.create(device, openni::SENSOR_COLOR);
		set_maxres(color);				
	} else {
		printf("Couldn't create color stream\n%s\n", openni::OpenNI::getExtendedError());
		exit(1);
	}

	
	char tmpfile[256];	
	
	while(1) {
		
		printf("Press Enter to record and send a segment (q to quit): ");
		
		fflush(stdin);
		if(getc(stdin) == 'q')
			break;
		
		time_t t = time(NULL);
		strftime(tmpfile, sizeof(tmpfile), "rgbd_%Y%m%d_%H-%M-%S.ply", localtime(&t));
		
		RawData raw(depth.getVideoMode().getResolutionX(), depth.getVideoMode().getResolutionY(),
					color.getVideoMode().getResolutionX(), color.getVideoMode().getResolutionY());
				
		printf("Recording started.\n");
		
		openni::VideoStream* streams[] = {&depth, &color};
		capture(streams, 2, raw, conf.capture_time);
			
		printf("Recording ended.\n");
		
		
		PointCloud cloud(raw.dresx*raw.dresy);
		depth_to_pointcloud(cloud, raw, depth, color);
		export_to_ply(tmpfile, cloud);
		
		printf("\nExtracted to point cloud: %s\n", tmpfile);
		
		send_file(curl, tmpfile, conf.url, conf.username, conf.password);
	}
	
	
	depth.destroy();
	color.destroy();
	device.close();
	openni::OpenNI::shutdown();
	
	delete[] conf.url;
	delete[] conf.username;
	delete[] conf.password;
}