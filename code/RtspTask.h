#pragma once
#include "GstTask.h"
#include <string>
#include "PacketQueue.h"
#include "../common/ts_thread.h"
using namespace std;

class TaskFlow;
class RtspTask :
	public BaseGstTask,ts_thread
{
public:
	RtspTask(TaskFlow* pFlow);
	~RtspTask();
public:
	static void* startThread(TaskFlow* pFlow);
	void stop(int code=1);

protected:
	virtual bool onStart();
	virtual void run();

public:
	int buildPipeline1();
	int buildPipeline();
	int buildUsbPipeline();
	int buildRtmpPipeline();
	void releasePileline();
	int addAudioBranch();
	struct FrameData * pullRawvideo();
	Packet * pullRawvideo1();
	void reportStatus();
	int addFakeBranch();

	void myfps(int deltatm);

	int oldfps = 0;
	int curfps = 0;
	int frameCount = 0;
	long long int lasttm = 0;
	int changeCount = 0;


public:
	static GstPadProbeReturn appsink_query_cb(GstPad * pad G_GNUC_UNUSED, GstPadProbeInfo * info, gpointer user_data G_GNUC_UNUSED);
	static GstPadProbeReturn pad_probe(GstPad * pad, GstPadProbeInfo * info, gpointer udata);
	static GstPadProbeReturn pad_probe2(GstPad* pad, GstPadProbeInfo* info, gpointer udata);
	static GstPadProbeReturn pad_probe1(GstPad * pad, GstPadProbeInfo * info, gpointer udata);
	static GstFlowReturn on_video_sample(GstElement* sink, gpointer udata);
	static GstFlowReturn on_audio_sample(GstElement* sink, gpointer udata);
	static gboolean onBusMessage(GstBus *bus, GstMessage *msg, gpointer data);
	static gboolean autoplug_continue_callback(GstElement * bin,GstPad * pad,GstCaps * caps,gpointer udata);
	static gboolean select_stream_callback(GstElement * rtspsrc, guint num, GstCaps * caps, gpointer udata);
	static gint * autoplug_select_callback(GstElement * bin,GstPad * pad,GstCaps * caps,GstElementFactory * factory,gpointer udata);
	static void on_dec2conv_link(GstElement *element, GstPad *pad, gpointer data);
	static void on_rtsp2dec_link(GstElement *element, GstPad *pad, gpointer data);
	static void on_rtsp2demux_link(GstElement* element, GstPad* pad, gpointer data);
	static void on_sdp_callback(GstElement * rtspsrc, GstSDPMessage * sdp, gpointer udata);
	static GstPadProbeReturn cb_have_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

	static void on_dec2conv_link1(GstElement *element, GstPad *pad, gpointer data);
	static void on_flv2dec_link(GstElement *element, GstPad *pad, gpointer data);
	
public:
	int rtsp_rtp_transmode_ = TCP_CONN_MODE;// DEFAULT_CONN_MODE;
	TaskFlow* pFlow_;
	//string strUrl_;

	GstElement* rtpdepay_ = NULL;
	GstElement* h26Xparse_ = NULL;

	GstElement *rtspsrc_ = NULL;
	GstElement *videodecoder_ = NULL;
	GstElement *audiodecoder_ = NULL;
	GstElement *videoqueue_ = NULL;
	GstElement *appvideosink_ = NULL;
	GstElement *vconverter_ = NULL;
	GstElement *aconverter_ = NULL;
	GstElement *audioqueue_ = NULL;
	GstElement *appaudiosink_ = NULL;
	GstElement *fakesink_ = NULL;
	int cur_resolution_width_ = 0;
	int cur_resolution_hetight_ = 0;
	unsigned framecount_ = 0;
	GstElement * cpasfilter_ = NULL;
	GstElement * videoscale_ = NULL;
	GstElement * audiocpasfilter_ = NULL;
	GstElement * audioresamp_ = NULL;
	GstElement * audiorate_;

	GstElement *flvdemux_ = NULL;

	int withaudio_ = 0;
	int needteardown_ = 0;
	int closestyle_ = 0;
	string closeMessage_ = "None";
	int cur_video_pts_ =0;
	int cur_seq_ = 0;
	unsigned long lastAudioStmp_ = 0;
	unsigned long lastVideoStmp_ = 0;
	int tmStamp_ms = 0;

	long long int last_pts_ = 0;
	int is_video_added = 0;
	int is_audio_added = 0;

	int need_measure_fps_ = 0;
	unsigned long video_start_ms_=0;

	int m_source_width=0;
	int m_source_height=0;
};

