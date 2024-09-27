#pragma once
#include "GstTask.h"
#include <string>
#include "PacketQueue.h"
#include "../common/ts_thread.h"
using namespace std;

class TaskFlow;
class RtpTask :
	public BaseGstTask,ts_thread
{
public:
	RtpTask(TaskFlow* pFlow);
	~RtpTask();
public:
	static void* startThread(TaskFlow* pFlow);
	void stop();

protected:
	virtual bool onStart();
	virtual void run();

public:
	int buildPipeline();
	void releasePileline();
	int addAudioBranch();
	void reportStatus();
	int addVideocodec(int pt);

	int parseSDP();




public:
	static GstPadProbeReturn appsink_query_cb(GstPad * pad G_GNUC_UNUSED, GstPadProbeInfo * info, gpointer user_data G_GNUC_UNUSED);
	static GstPadProbeReturn pad_probe(GstPad * pad, GstPadProbeInfo * info, gpointer udata);
	static GstPadProbeReturn pad_probe1(GstPad * pad, GstPadProbeInfo * info, gpointer udata);
	static GstFlowReturn on_video_sample(GstElement* sink, gpointer udata);
	static GstFlowReturn on_audio_sample(GstElement* sink, gpointer udata);
	static gboolean onBusMessage(GstBus *bus, GstMessage *msg, gpointer data);
	static void on_dec2conv_link(GstElement *element, GstPad *pad, gpointer data);

	static void on_rtpbin2dec_link(GstElement *element, GstPad *pad, gpointer data);
	static GstCaps * onClockrateRequest(GstElement * buffer, guint sessionid, guint pt,gpointer udata);

	

	
	
	
public:
	int rtsp_rtp_transmode_ = TCP_CONN_MODE;// DEFAULT_CONN_MODE;
	TaskFlow* pFlow_;
	//string strUrl_;

	GstElement *trans_layer_src_ = NULL;
	GstElement *rtpbin_ = NULL;
	GstElement *videodepay_ = NULL;
	GstElement *appvideosink_ = NULL;
	GstElement *videodec_ = NULL;
	GstElement *vconverter_ = NULL;


	GstElement *audiodecoder_ = NULL;
	GstElement *aconverter_ = NULL;
	GstElement *audioqueue_ = NULL;
	GstElement *appaudiosink_ = NULL;
	GstElement *fakesink = NULL;
	int cur_resolution_width_ = 0;
	int cur_resolution_hetight_ = 0;
	unsigned framecount_ = 0;
	GstElement * cpasfilter_ = NULL;
	GstElement * videoscale_ = NULL;
	GstElement * audiocpasfilter_ = NULL;
	GstElement * audioresamp_ = NULL;
	GstElement * audiorate_;

	/*config info from sdp from gb28181*/
	int setup_type_;//1, active 0,passive
	int passive_trans_; //0 udp, 1 tcp
	int listen_port;
	int peer_port;
	string peer_host;
	string  video_codec_;
	string ssrc_;
	int video_pt;
	int audio_pt;

	int withaudio_ = 0;
	int needteardown_ = 0;
	int closestyle_ = 0;
	string closeMessage_ = "None";
	int cur_video_pts_ =0;
	int cur_seq_ = 0;
	unsigned long lastAudioStmp_ = 0;


	int is_video_added = 0;
	int is_audio_added = 0;
	
};

