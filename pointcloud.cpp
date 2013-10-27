#include <cstdio>
#include <OpenNI.h>
#include <cmath>

#include "pointcloud.h"
#include "capture.h"

PointCloud::PointCloud(int num) {
	num = num;
	
	x = new float[num];
	y = new float[num];
	z = new float[num];
	r = new uint8_t[num];
	g = new uint8_t[num];
	b = new uint8_t[num];
		
}

PointCloud::~PointCloud() {
	delete[] x;
	delete[] y;
	delete[] z;
	delete[] r;
	delete[] g;
	delete[] b;	
}


void depth_to_pointcloud(PointCloud &cloud, RawData &raw, openni::VideoStream &depthstrm, openni::VideoStream &clrstrm, float maxdepth) {
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
			
			if(avgdepth > maxdepth*1000.f)
				continue;
			
			openni::CoordinateConverter::convertDepthToWorld(depthstrm, (float)x, (float)y, avgdepth, &cloud.x[i], &cloud.y[i], &cloud.z[i]);
			
			int coffx = 0, coffy = 0, tmp1, tmp2;
			clrstrm.getCropping(&coffx, &coffy, &tmp1, &tmp2);
			
			int cx = x/(float)raw.dresx*raw.cresx-coffx;
			int cy = y/(float)raw.dresy*raw.cresy-coffy;
			
			if(cx >= raw.cresx)
				cx--;
			if(cy >= raw.cresy)
				cy--;
			
			int cidx = cx+cy*raw.cresx;
			
			cloud.r[i] = raw.r[cidx]/(float)raw.cframenum;
			cloud.g[i] = raw.g[cidx]/(float)raw.cframenum;
			cloud.b[i] = raw.b[cidx]/(float)raw.cframenum;
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