#include <cstdio>
#include <OpenNI.h>
#include <cmath>

#include "pointcloud.h"
#include "capture.h"

PointCloud::PointCloud(int num) {
	this->num = num;
	
	this->x = new float[num];
	this->y = new float[num];
	this->z = new float[num];
	this->r = new uint8_t[num];
	this->g = new uint8_t[num];
	this->b = new uint8_t[num];
		
}

PointCloud::~PointCloud() {
	delete[] this->x;
	delete[] this->y;
	delete[] this->z;
	delete[] this->r;
	delete[] this->g;
	delete[] this->b;	
}


void depth_to_pointcloud(PointCloud &cloud, RawData &raw, openni::VideoStream &depthstrm, openni::VideoStream &clrstrm) {
	int x, y;
	
	float avgdepth;
	int clrx;
	int clry;
	int i = 0;
	
	for(y = 0; y < raw.dresy; y++) {
		for(x = 0; x < raw.dresx; x++) {
			if(raw.dframenums[x+y*raw.dresx] == 0)
				continue;
			
			avgdepth = raw.d[x+y*raw.dresx]/(float)raw.dframenums[x+y*raw.dresx];
							
			openni::CoordinateConverter::convertDepthToWorld(depthstrm, (float)x, (float)y, avgdepth, &cloud.x[i], &cloud.y[i], &cloud.z[i]);
			
			int cx = x/(float)raw.dresx*raw.cresx;
			int cy = y/(float)raw.dresy*raw.cresy;
			
			if(cx >= raw.cresx)
				cx--;
			if(cy >= raw.cresy)
				cy--;
			
			cloud.r[i] = raw.r[cx+cy*raw.cresx]/(float)raw.cframenum;
			cloud.g[i] = raw.g[cx+cy*raw.cresx]/(float)raw.cframenum;
			cloud.b[i] = raw.b[cx+cy*raw.cresx]/(float)raw.cframenum;
			i++;
			
// 			cloud.z[i]*=-1;
		}
		
		printf("\r%.1f%%", y/(float)(raw.dresy-1)*100.0);
	}
	
	cloud.num = i;
}

void export_to_ply(char *filename, PointCloud &c) {
	FILE *f = fopen(filename, "wb");
	
	fprintf(f, "ply\n"
			   "format ascii 1.0\n"
			   "comment created by rgbdsend\n"
			   "element vertex %d\n"
			   "property float32 x\n"
			   "property float32 y\n"
			   "property float32 z\n"
			   "property uint8 red\n"
			   "property uint8 green\n"
			   "property uint8 blue\n"
			   "element face 0\n"
			   "property list uint8 int32 vertex_indices\n"
			   "end_header\n", c.num);
	
	for(int i = 0; i < c.num; i++)
		fprintf(f, "%f %f %f %d %d %d\n", c.x[i], c.y[i], c.z[i], c.r[i], c.g[i], c.b[i]);
	
	fclose(f);
}