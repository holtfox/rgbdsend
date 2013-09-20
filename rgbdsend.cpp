#include <cstdio>
#include <ctime>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <OpenNI.h>
#include <curl/curl.h>


namespace rgbdsend {
	const int read_timeout = 1000;
	
	const char *config_file_name = "config";
}

struct Config {
	char *url;
	
	char *username;
	char *password;
	
	int capture_time; // in clocks
};

char curl_errbuf[CURL_ERROR_SIZE];

int readfile_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
	FILE *file = (FILE *)userdata;
	
	return fread(ptr, size, nmemb, file);
}

void send_file(CURL *curl, char *filename, char *url, char *user, char *password) {	
	FILE *file = fopen(filename, "r");
	
	if(file == NULL) {
		printf("Upload Error: Could't read '%s': %s.\n", filename, strerror(errno));
		return;
	}	
	
	int fsize;
	
	fseek(file, 0, SEEK_END);
	fsize = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	int userlen = strlen(user);	
	char *buf = new char[userlen+strlen(password)+2];
	strcpy(buf, user);
	buf[userlen] = ':';
	strcpy(buf+userlen+1, password);
	
	curl_easy_setopt(curl, CURLOPT_USERPWD, buf);
	
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, readfile_callback);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_READDATA, file);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t) fsize);
		
	if(curl_easy_perform(curl) != 0) {
		printf("Upload Error: %s.\n", curl_errbuf);
	}	
	
	curl_easy_reset(curl);
	
	fclose(file);
	delete[] buf;
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

CURL *init_curl() {
	curl_global_init(CURL_GLOBAL_ALL);
	return curl_easy_init();	
}

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
		rc = depth.create(device, openni::SENSOR_DEPTH);
		if(rc == openni::STATUS_OK)	{
			rc = depth.start();
			if(rc != openni::STATUS_OK)	{
				printf("Couldn't start the color stream\n%s\n", openni::OpenNI::getExtendedError());
			}
		}		
	} else {
		printf("Couldn't create depth stream\n%s\n", openni::OpenNI::getExtendedError());
		
		exit(1);
	}

	if(device.getSensorInfo(openni::SENSOR_COLOR) != NULL) {
		rc = color.create(device, openni::SENSOR_COLOR);
		if(rc == openni::STATUS_OK)	{
			rc = color.start();
			if(rc != openni::STATUS_OK)	{
				printf("Couldn't start the color stream\n%s\n", openni::OpenNI::getExtendedError());
			}
		}		
	} else {
		printf("Couldn't create color stream\n%s\n", openni::OpenNI::getExtendedError());
		exit(1);
	}

	set_maxres(depth);
	set_maxres(color);
	
	char tmpfile[256];	
	
	while(1) {
		int starttime;
		
		printf("Press Enter to record and send a segment (q to quit): ");
		
		fflush(stdin);
		if(getc(stdin) == 'q')
			break;
				
		starttime = clock();
		printf("Recording started.\n");
		
		openni::Recorder recorder;
		time_t t;
		t = time(NULL);
		
		strftime(tmpfile, sizeof(tmpfile), "rgbd_%Y%m%d_%H-%M.oni", localtime(&t));
		printf("%s", tmpfile);
		recorder.create(tmpfile);
		recorder.attach(color);
		recorder.attach(depth);		
		recorder.start();
		while(clock()-starttime < conf.capture_time) {
			usleep(100);
		}
		
		recorder.stop();
		recorder.destroy();
		printf("Recording ended.\n");
		
		send_file(curl, tmpfile, conf.url, conf.username, conf.password);
	}
	
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	depth.stop();
	color.stop();
	depth.destroy();
	color.destroy();
	device.close();
	openni::OpenNI::shutdown();
	
	delete[] conf.url;
	delete[] conf.username;
	delete[] conf.password;
}