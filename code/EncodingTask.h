#pragma once
#include "GstTask.h"
#include "../common/ts_thread.h"
#include "PacketQueue.h"
#include "../common/ts_json.h"
#include <string>
using namespace std;

class TaskFlow;
class StatusQueue;
class EncodingTask :
	public BaseGstTask, ts_thread
{
	EncodingTask(TaskFlow* pFlow);
	virtual ~EncodingTask();
public:
	static void* startThread(TaskFlow* pFlow);
	void stop(int type=1);
protected:
	virtual bool onStart();
	virtual void run();
private:
	int buildFilePipeline();
	int buildRtspPipeline();
	int buildRtpPipeline();
	int buildRtmpPipeline();
	int buildRawPipeline();

	int addAudioBranch();
	int addRtspAudioBranch();
	int addRtpAudioBranch();
	bool getVideoFrame();
	bool getAudioFrame();
	void reportStatus();
	int selectVideoEncoder();
	int addVideoSrc();
	static gboolean onBusMessage(GstBus *bus, GstMessage *msg, gpointer data);
	void setExtraParameter(GstElement *, TSJson::Value&);
	int parseSDP();
public:
	static gboolean push_vidoedata(gpointer *data);
	static gboolean push_audiodata(gpointer *data);
	static GstFlowReturn on_video_frame(GstElement* sink, gpointer udata);
	static GstPadProbeReturn cb_have_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
	static void handle_request_callback(GstElement * rtsp_client_sink, GstRTSPMessage * request, GstRTSPMessage * response, gpointer udata);
//private:
public:
	TaskFlow * pFlow_;

	GstElement *audioappsrc_ = NULL;
	GstElement *audioresample_ = NULL;
	GstElement *audioencoder_ = NULL;
	GstElement *audioparser_ = NULL;
	GstElement *videoappsrc_ = NULL;
	GstElement *videoencoder_ = NULL;
	GstElement *videoparser_ = NULL;
	GstElement *avmuxer_ = NULL;
	GstElement *streamsink_ = NULL;
	GstElement *streamsink1_ = NULL;

	GstElement *queue_ = NULL;
	GstElement *queue1_ = NULL;
	GstElement *queue2_ = NULL;

	GstElement *vconverter_ = NULL;
	GstElement * audiocpasfilter_ = NULL;
	GstElement * aaccpasfilter_ = NULL;
	GstElement * videocpasfilter_ = NULL;
	GstElement * aconverter_ = NULL;
	GstElement* cpasfilter_ = NULL;
	


	int video_frame_count_ = 1;;
	int audio_frame_count_ = 1;
	int curWidth = 0;
	int curHeight = 0;
	guint getVideotimer_=0;
	guint getAudiotimer_ =0;
	int withaudio_ = 0;
	int tid_;
	int isStart_ = 0;
	int closestyle_ = 0;
	string closeMessage_;
	int cur_video_pts_ = 0;
	int cur_seq_ = 0;
	int statusSent_ = 0;

	int setup_type_;//1, active 0,passive
	int passive_trans_; //0 udp, 1 tcp
	int listen_port;
	int peer_port;
	string peer_host;
	string  video_codec_;
	string ssrc_;
	int video_pt;
	int audio_pt;
	int default_fps_n_254_ = 0;

	FILE* pfile = NULL;

	
};

