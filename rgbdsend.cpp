#include <cstdio>
#include <ctime>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <OpenNI.h>
#include <queue>

#include "rgbdsend.h"
#include "capture.h"
#include "pointcloud.h"
#include "network.h"
#include "config.h"

bool record_oni(char *tmpfile, int bufsize, openni::VideoStream &depth, openni::VideoStream &color, Config &conf) {
	openni::Recorder recorder;
		
	time_t t = time(NULL);
	strftime(tmpfile, bufsize, "rgbd_%Y%m%d_%H-%M-%S.oni", localtime(&t));
	printf("Starting ONI Capture.\n");
	depth.start();
	color.start();
	openni::Status rc = recorder.create(tmpfile);	
	if(rc != openni::STATUS_OK) {
		printf("Error: Failed to open '%s' for writing!\n%s", tmpfile, openni::OpenNI::getExtendedError());
		return false;
	}
	
	recorder.attach(color);
	recorder.attach(depth);
	recorder.start();
	
	struct timespec	start, tp;
	clock_gettime(CLOCK_MONOTONIC, &start);
	
	long tt;
	do {
		usleep(100);
		clock_gettime(CLOCK_MONOTONIC, &tp);
		tt = (tp.tv_sec-start.tv_sec)*1000+(tp.tv_nsec-start.tv_nsec)/1000000;
	} while(tt < conf.capture_time);
		
	
	recorder.stop();
	color.stop();
	depth.stop();
	recorder.destroy();
	
	printf("Captured ONI to '%s'\n", tmpfile);
	
	return true;
}

void oni_to_pointcloud(char *tmpfile, Config &conf) {
	openni::Device onidev;
	openni::VideoStream depth, color;
	
	if(!init_openni_device(tmpfile, &onidev, &depth, &color)) {
		return;
	}
		
	int fnlen = strlen(tmpfile);
	tmpfile[fnlen-3] = 'p';
	tmpfile[fnlen-2] = 'l';
	tmpfile[fnlen-1] = 'y';
	
	onidev.getPlaybackControl()->setRepeatEnabled(false);
	
	int dw, dh, cw, ch;
	int tmp1, tmp2;
	
	if(!depth.getCropping(&tmp1, &tmp2, &dw, &dh)) {
		dw = depth.getVideoMode().getResolutionX();
		dh = depth.getVideoMode().getResolutionY();
	}
	
	if(!color.getCropping(&tmp1, &tmp2, &cw, &ch)) {
		cw = color.getVideoMode().getResolutionX();
		ch = color.getVideoMode().getResolutionY();
	}
	
	RawData raw(dw, dh, cw, ch);
			
// 	printf("Recording started.\n");
	
	openni::VideoStream* streams[] = {&depth, &color};
	int framecounts[] = {onidev.getPlaybackControl()->getNumberOfFrames(depth),
						 onidev.getPlaybackControl()->getNumberOfFrames(color)};
						 
	if(framecounts[0] == 0 && framecounts[1] == 0) {
		printf("Error: ONI didn't contain any frames.\n");
	} else {
		capture(streams, 2, raw, framecounts);
			
	// 	printf("Recording ended.\n");
			
		PointCloud cloud(raw.dresx*raw.dresy);
		depth_to_pointcloud(cloud, raw, depth, color, conf.capture_max_depth);
		export_to_ply(tmpfile, cloud);
		
		printf("\nExtracted to point cloud: %s\n", tmpfile);
	}
	depth.destroy();
	color.destroy();
	onidev.close();
}

void process_onis(std::queue<char *> &filelist, CURL *curl, Config &conf) {
	printf("Started processing captured onis...\n");
	
	while(!filelist.empty()) {
		printf("%s\n", filelist.front());
		oni_to_pointcloud(filelist.front(), conf);		
		
		if(conf.dest_url && conf.dest_username && conf.dest_password)
			send_file(curl, filelist.front(), conf.dest_url, conf.dest_username, conf.dest_password);
		else
			printf("No destination server specified. Skipping transfer.\n");
		
		char *p = strrchr(filelist.front(), '.');
		p[1] = 'o';
		p[2] = 'n';
		p[3] = 'i';
		
		remove(filelist.front());
		
		delete[] filelist.front();
		filelist.pop();
	}
	
	printf("Done processing.\n");
}

static openni::Device __device; // have to be global to be reachable by atexit().
static openni::VideoStream __depth, __color;

static void atexit_handler() {
	printf("Closing devices.\n");
	cleanup_openni(__device, __depth, __color);
	printf("Terminating.\n");
}

int main(int argc, char **argv) {
	openni::Device &device = __device;
	openni::VideoStream &depth = __depth, &color = __color;
	openni::Status rc;
		
	char *prefix = strrchr(argv[0], '/')+1;
	char *cfgfile = new char[prefix-argv[0]+strlen(rgbdsend::config_file_name)+1];
	
	strncpy(cfgfile, argv[0], prefix-argv[0]+1);
	strcpy(cfgfile+(prefix-argv[0]), rgbdsend::config_file_name);
		
	Config conf;
	if(conf.read(cfgfile) != 1) {
		printf("Config: Falling back to builtin presets.\n");
		conf.setDefaults();
	}
		
	delete[] cfgfile;
		
	CURL *curl = init_curl();
	
	Daemon daemon;
	daemon.init(conf.daemon_port, conf.daemon_timeout);
		
	atexit(atexit_handler);
	
	init_openni(&device, &depth, &color, conf);
	
	int dw, dh, cw, ch;
	int tmp1, tmp2;
	
	if(!depth.getCropping(&tmp1, &tmp2, &dw, &dh)) {
		dw = depth.getVideoMode().getResolutionX();
		dh = depth.getVideoMode().getResolutionY();
	}
	
	if(!color.getCropping(&tmp1, &tmp2, &cw, &ch)) {
		cw = color.getVideoMode().getResolutionX();
		ch = color.getVideoMode().getResolutionY();
	}
	
	printf("Resolution:\nDepth: %dx%d @ %d fps\nColor: %dx%d @ %d fps\n",
		   dw, dh, depth.getVideoMode().getFps(),
		   cw, ch, color.getVideoMode().getFps());
	
	std::queue<char *> onilist;
	
	Command cmd;
	while(1) {
		timeval t;
		t.tv_sec = conf.daemon_timeout;
		t.tv_usec = 0;
		
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(daemon.sock, &fds);
		FD_SET(daemon.csock, &fds);
		
		int in = select((daemon.csock > daemon.sock ? daemon.csock : daemon.sock)+1, &fds, 0, 0, &t);
				
		if(FD_ISSET(daemon.sock, &fds))
			daemon.acceptConnection();
		
		if(FD_ISSET(daemon.csock, &fds)) {
			char b[5];
			int r = daemon.receiveCommand(&cmd);
			
			if(r == 0) {
				printf("Daemon Error: Could not receive command.\n");
				
				daemon.closeConnection();
				continue;
			}
			
			if(r == 2) // keep alive
				continue;			
									
			if(strncmp(cmd.header, "capt", 4) == 0) {
				printf("Received capture command.\n");
				
				char *newfile = new char[rgbdsend::filename_bufsize];
				if(record_oni(newfile, rgbdsend::filename_bufsize, depth, color, conf) == true) {
					onilist.push(newfile);
				} else {
					delete[] newfile;
				}
				daemon.sendCommand("okay", 0, 0);
			} else if(strncmp(cmd.header, "thmb", 4) == 0) {
				printf("Received thumbnail command.\n");
				unsigned char *thumbbuf = NULL;
				long unsigned int size = 0;
				capture_thumbnail(&thumbbuf, &size, color);
				printf("Captured thumbnail. %ld bytes\n", size);
				
				daemon.sendCommand("stmb", thumbbuf, size);
				
//				delete[] thumbbuf;
			} else if(strncmp(cmd.header, "quit", 4) == 0) {
				daemon.closeConnection();
			} else {
				printf("Daemon Error: Received undefined command.\n");
			}
		}
				
		if(daemon.csock != -1 && in <= 0) {
			daemon.closeConnection();			
		}
		
		if(daemon.csock == -1 && !onilist.empty())
			process_onis(onilist, curl, conf);
	}
		
	cleanup_curl(curl);
	cleanup_openni(device, depth, color);
}
