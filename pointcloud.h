#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include <stdint.h>

namespace openni {
	class VideoStream;
}

class RawData;

struct PointCloud {
public:
	PointCloud(int num);
	~PointCloud();
	
	float *x;
	float *y;
	float *z;
	
	uint8_t *r;
	uint8_t *g;
	uint8_t *b;
	
	int num;
};

void depth_to_pointcloud(PointCloud &cloud, RawData &raw, openni::VideoStream &depthstrm, openni::VideoStream &clrstrm);

void export_to_ply(char *filename, PointCloud &c);

#endif