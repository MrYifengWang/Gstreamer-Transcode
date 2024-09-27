#pragma once

#if defined(NO_ENHANCE)
#include "GstTask.h"
#include "../common/ts_thread.h"
class TaskFlow;
class EnhanceTask : public ts_thread
{
	EnhanceTask(TaskFlow* pFlow);
	virtual ~EnhanceTask();
public:
	static void* startThread(TaskFlow* pFlow);
	void stop();
protected:
	virtual bool onStart();
	virtual void run();
};
#else


#include "GstTask.h"
#include "../common/ts_thread.h"
#include "PacketQueue.h"
#include <nvVideoEffects.h>
#include <nvCVImage.h>
#include <nvCVStatus.h>
#include <cuda_runtime_api.h>
#include <string>
#include "opencv2/opencv.hpp"

using namespace std;

class TaskFlow;
class RtspTask;
class StatusQueue;
class VideoFilter {
public:
	VideoFilter();
	VideoFilter(char*, string& modpath);
	virtual ~VideoFilter();

public:
	int init( Packet* frame);
	int processOneFrame();
	int upLink(VideoFilter*);
	int downLink(VideoFilter*);
	void release();
	int outputYUV(cv::Mat& mat);

public:
	string modelpath_;
	string effcname;
	CUstream stream=0;
	float FLAG_strength = 0.f;
	void* state = nullptr;
	void* stateArray[1];
	NvVFX_Handle  _eff=NULL;
	NvCVImage     _srcGpuBuf;
	NvCVImage     _dstGpuBuf;

	cv::Mat       _srcImg;
	cv::Mat       _dstImg;
	NvCVImage     _srcVFX;
	NvCVImage     _dstVFX;
	NvCVImage     _tmpVFX;

};
class EnhanceTask : public ts_thread
{
	EnhanceTask(TaskFlow* pFlow);
	virtual ~EnhanceTask();
public:
	static void* startThread(TaskFlow* pFlow);
	void stop();
protected:
	virtual bool onStart();
	virtual void run();

private:
	int buildFilter();
	int buildFilter1(char*);
	NvCV_Status allocBuffers1(unsigned width, unsigned height);
	void processDenoise();
	void processDenoiseCV(char*);
	void processChain(char* a, char* b);
	int processImage(const char *inFile, const char *outFile);
	NvCV_Status allocBuffers(unsigned width, unsigned height);
	NvCV_Status allocTempBuffers();

	bool RGB24_TO_YUV420(unsigned char *RgbBuf, int w, int h, unsigned char *yuvBuf);
	bool yuv420ToRgb(char *yuv, int w, int h, char *rgb);
	void NV12_To_RGB(unsigned int width, unsigned int height, unsigned char *yuyv, unsigned char *rgb);
	void yuv2bgr(Packet* frame);

	int initChain(Packet* frame);
private:
	TaskFlow* pFlow_;
	bool chainInited = false;
	VideoFilter* vChain[4];

	CUstream stream=0;
	float       FLAG_strength = 0.f;
	void* state = nullptr;
	void* stateArray[1];


	NvVFX_Handle  effectHandle;
	unsigned char * bgrbuf=NULL;
	unsigned char * dstbgrbuf = NULL;



	NvVFX_Handle  _eff = NULL;
	cv::Mat       _srcImg;
	cv::Mat       _dstImg;
	NvCVImage     _srcGpuBuf;
	NvCVImage     _dstGpuBuf;
	NvCVImage     _srcVFX;
	NvCVImage     _dstVFX;
	NvCVImage     _tmpVFX; 
	bool _show = false;
	bool _inited = false;
};

#endif