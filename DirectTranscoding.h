#pragma once
#include "GstTask.h"
#include <string>
#include "PacketQueue.h"
#include "../common/ts_json.h"
#include "../common/ts_thread.h"
using namespace std;

class TaskFlow;
class DirectTranscodingTask :
	public BaseGstTask, ts_thread
{
public:
	DirectTranscodingTask(TaskFlow* pFlow);
	~DirectTranscodingTask();
public:
	static void* startThread(TaskFlow* pFlow);
	void stop(int code=1);

protected:
	virtual bool onStart();
	virtual void run();

public:
	int buildRtsp2RtspPipeline();
	int buildRtsp2RtspPipeline2();

	int initRtspPipeline();
	int addPipelineDecoder();
	int addPipelineRtsp();
	int addPipelineRtmp();
	int addPipelineFile();
	int addPipelineRtp();

	int completeRtspPipeline();

	int addVideoBranch(int type);
	int addAudioBranch(int type);
	int selectVideoEncoder();



	void releasePileline();
	void reportStatus();

	void setExtraParameter(GstElement*, TSJson::Value&);


	void myfps(int deltatm);

	int oldfps = 0;
	int curfps = 0;
	int frameCount = 0;
	long long int lasttm = 0;
	int changeCount = 0;


public:
	static GstPadProbeReturn appsink_query_cb(GstPad* pad G_GNUC_UNUSED, GstPadProbeInfo* info, gpointer user_data G_GNUC_UNUSED);
	static GstPadProbeReturn pad_probe(GstPad * pad, GstPadProbeInfo * info, gpointer udata);
	static GstPadProbeReturn pad_probe2(GstPad* pad, GstPadProbeInfo* info, gpointer udata);
	static GstPadProbeReturn pad_probefps(GstPad* pad, GstPadProbeInfo* info, gpointer udata);

	static GstPadProbeReturn pad_probe1(GstPad * pad, GstPadProbeInfo * info, gpointer udata);
	static gboolean onBusMessage(GstBus *bus, GstMessage *msg, gpointer data);
	
	static void on_sdp_callback(GstElement* rtspsrc, GstSDPMessage* sdp, gpointer udata);
	static void on_rtsp2dec_link(GstElement* element, GstPad* pad, gpointer data);



public:
	TaskFlow* pFlow_;

	GstElement* fakesink_ = NULL;
	GstElement *streamsrc_ = NULL;
	GstElement *demuxer_ = NULL;
	GstElement *videoparser_ = NULL;
	GstElement* audiodemuxer_ = NULL;

	GstElement *audioparser_ = NULL;
	GstElement *videodecoder_ = NULL;
	GstElement *audiodecoder_ = NULL;

	GstElement* videoencoder_;
	GstElement* audioencoder_;

	GstElement* videoparserout_ = NULL;

	GstElement* avmuxer_;
	GstElement* streamsink_ = NULL;

	GstElement* videoqueue_ = NULL;

	GstElement *videoqueue1_ = NULL;
	GstElement *vconverter_ = NULL;
	GstElement *aconverter_ = NULL;
	GstElement* aconverter1_ = NULL;

	GstElement *audioqueue_ = NULL;

	GstElement* appaudiosink_ = NULL;
	GstElement* rtsppushsink_ = NULL;
	GstElement* streamsink1_ = NULL;

	

	string closeMessage_;

	GstElement* queue2_ = NULL;
	GstElement* queue1_ = NULL;

	GstElement * cpasfilter_ = NULL;
	GstElement * audiocpasfilter_ = NULL;
	GstElement * audioresamp_ = NULL;
	GstElement *flvdemux_ = NULL;
	GstElement * audiorate_;
	GstElement* videorate_;



	int audio_pt_=0;
	int video_pt_=0;


	int cur_resolution_width_ = 0;
	int cur_resolution_hetight_ = 0;
	unsigned framecount_ = 0;
	int withaudio_ = 0;
	int needteardown_ = 0;
	int closestyle_ = 0;
	int cur_video_pts_ = 0;
	int cur_seq_ = 0;

	int is_video_added = 0;
	int is_audio_added = 0;
};

