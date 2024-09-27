#pragma once
#include "GstTask.h"
#include <string>
#include "PacketQueue.h"
#include "../common/ts_thread.h"
using namespace std;

class TaskFlow;
class FilesrcTask :
	public BaseGstTask, ts_thread
{
public:
	FilesrcTask(TaskFlow* pFlow);
	~FilesrcTask();
public:
	static void* startThread(TaskFlow* pFlow);
	void stop();

protected:
	virtual bool onStart();
	virtual void run();

public:
	int buildMp4Pipeline();
	int buildTsPipeline();
	int buildFastPipeline();
	int buildRawPipeline();
	int buildRawPipeline1();

	void releasePileline();
	int addAudioBranch();
	void reportStatus();
	int selectVideoEncoder();




public:
	static GstPadProbeReturn appsink_query_cb(GstPad * pad G_GNUC_UNUSED, GstPadProbeInfo * info, gpointer user_data G_GNUC_UNUSED);
	static GstPadProbeReturn pad_probe(GstPad * pad, GstPadProbeInfo * info, gpointer udata);
	static GstPadProbeReturn pad_probe1(GstPad * pad, GstPadProbeInfo * info, gpointer udata);
	static GstFlowReturn on_video_sample(GstElement* sink, gpointer udata);
	static GstFlowReturn on_audio_sample(GstElement* sink, gpointer udata);
	static gboolean onBusMessage(GstBus *bus, GstMessage *msg, gpointer data);

	static void on_dec2conv_link(GstElement *element, GstPad *pad, gpointer data);
	static void on_demuxer2dec_link(GstElement *element, GstPad *pad, gpointer data);



public:
	TaskFlow* pFlow_;
	//string strUrl_;
	GstElement* tee_ = NULL;

	GstElement *filesrc_ = NULL;
	GstElement *filedemuxer_ = NULL;
	GstElement *videoparser_ = NULL;
	GstElement *audioparser_ = NULL;

	GstElement *videodecoder_ = NULL;
	GstElement *audiodecoder_ = NULL;
	GstElement *videoqueue_ = NULL;
	GstElement *appvideosink_ = NULL;
	GstElement *vconverter_ = NULL;
	GstElement *aconverter_ = NULL;
	GstElement *audioqueue_ = NULL;
	GstElement *appaudiosink_ = NULL;
	GstElement *fakesink = NULL;
	int cur_resolution_width_ = 0;
	int cur_resolution_hetight_ = 0;
	unsigned framecount_ = 0;
	GstElement * cpasfilter_ = NULL;
	GstElement * audiocpasfilter_ = NULL;
	GstElement * audioresamp_ = NULL;
	GstElement *flvdemux_ = NULL;
	GstElement * audiorate_;

	GstElement * videoencoder_;
	GstElement * avmuxer_;

	int withaudio_ = 0;
	int needteardown_ = 0;
	int closestyle_ = 0;
	int cur_video_pts_ = 0;
	int cur_seq_ = 0;

	int is_video_added = 0;
	int is_audio_added = 0;
};

