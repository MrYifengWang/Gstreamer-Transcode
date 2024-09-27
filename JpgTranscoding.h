#pragma once
#include "GstTask.h"
#include <string>
#include "PacketQueue.h"
#include "../common/ts_json.h"
#include "../common/ts_thread.h"
using namespace std;

class TaskFlow;
class JpgTranscodingTask :
	public BaseGstTask, ts_thread
{
public:
	JpgTranscodingTask(TaskFlow* pFlow);
	~JpgTranscodingTask();
public:
	static void* startThread(TaskFlow* pFlow);
	void stop();

protected:
	virtual bool onStart();
	virtual void run();

public:
	int buildJpg2JpgPipeline();
	void releasePileline();
	void reportStatus();
	static gboolean onBusMessage(GstBus* bus, GstMessage* msg, gpointer data);




public:
	TaskFlow* pFlow_;
	string closeMessage_;
	int closestyle_=0;


	GstElement *filesrc_ = NULL;
	GstElement* fileparse_ = NULL;
	GstElement *jpgdecoder_ = NULL;
	GstElement* jpgencoder_;
	GstElement* jpgsink_ = NULL;
	GstElement * cpasfilter_ = NULL;
	

	int cur_resolution_width_ = 0;
	int cur_resolution_hetight_ = 0;


};

