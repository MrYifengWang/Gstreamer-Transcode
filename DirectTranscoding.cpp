#include "DirectTranscoding.h"
#include "../common/ts_time.h"
#include "GstUtil.h"
#include "TaskFlow.h"
#include "TaskManager.h"
#include "../common/ts_time.h"
#include "define.h"
#include "../common/EdUrlParser.h"
#include "DebugLog.h"
static int  GST_AUTOPLUG_SELECT_TRY = 0;
static int GST_AUTOPLUG_SELECT_EXPOSE = 1;
static int  GST_AUTOPLUG_SELECT_SKIP = 2;

DirectTranscodingTask::DirectTranscodingTask(TaskFlow* pFlow)
{
	pFlow_ = pFlow;
	//strUrl_ = pFlow->rtspUrl_; //
	withaudio_ = pFlow->withaudio_;
	tid_ = TaskManager::total_tid++;
	gst_init(NULL, NULL);
	puts("create DirectTranscodingTask");
}

DirectTranscodingTask::~DirectTranscodingTask() {
	puts("~delete DirectTranscodingTask");
	//releasePileline();
	reportStatus();
	
	/*Packet* newData = new Packet();
	pFlow_->pRawData_in->push(newData);
	Packet* newData1 = new Packet();
	pFlow_->pAudioData_in->push(newData1);
	pFlow_->onComplete(2);*/

}

void DirectTranscodingTask::reportStatus()
{
	/*
		rtsp task closed
		*/
	TSJson::Value root;
	root["tid"] = pFlow_->taskid_;
	root["taskname"] = "decoding";
	root["tmstamp"] = ts_time::current();
	root["starttime"] = pFlow_->task_start_time_;

	if (closestyle_ == 0 || closestyle_ == 1) {
		root["code"] = ERR_NO_ERROR;
		root["message"] = "eos";

	}
	else if (closestyle_ == -2) {
		root["code"] = ERR_RTSP_PUSH_FAIL;
		root["message"] = closeMessage_;

	}
	else if (closestyle_ == 2) {
		root["code"] = ERR_RTSP_LOST;
		root["message"] = closeMessage_;

	}
	else if (closestyle_ == 3) {
		root["code"] = ERR_RTSP_PASSWORD;
		root["message"] = closeMessage_;

	}
	else if (closestyle_ == 5) {
		root["code"] = ERR_DECODE_ERR;
		root["message"] = "build pipeline fail, check gstreamer plugin on the device";

	}
	else if (closestyle_ == 8) {
		root["code"] = ERR_FPS_CHANGED;
		root["oldfps"] = oldfps;
		root["fps"] = curfps;
		root["message"] = "camera's fps changed";

	}

	TaskManager::GetInstance()->onAnyWorkStatus(root, 0);

}
void* DirectTranscodingTask::DirectTranscodingTask::startThread(TaskFlow* pFlow)
{
	DirectTranscodingTask* thread = new DirectTranscodingTask(pFlow);

	thread->setAutoDelete(true);
	thread->start();

	return thread;
}
void DirectTranscodingTask::stop(int code)
{

	closestyle_ = code;
	needteardown_ = 1;
	bool ret = gst_bus_post(bus_, gst_message_new_eos(GST_OBJECT_CAST(streamsrc_)));
	//printf("stop rtsp task--------------------%d %s \n", ret, pFlow_->taskid_.c_str());

}
bool DirectTranscodingTask::DirectTranscodingTask::onStart()
{
	return true;
}
void DirectTranscodingTask::DirectTranscodingTask::run()
{

	isRun = STATUS_CONNECTING;

	printf("start connect rtsp url: %s \n", pFlow_->rtspUrl_.c_str());


	int ret = 0;
	if (pFlow_->inputtype_ == "rtsp") {
		ret = initRtspPipeline();
		//completeRtspPipeline();
	}
	else if (pFlow_->inputtype_ == "rtmp") {
		puts("-----------------ts file src-------------------------");

		ret = buildRtsp2RtspPipeline();

	}

	if (ret < 0)
	{
		isRun = STATUS_DISCONNECT;
		DebugLog::writeLogF("rtsp build pipe line failed :%s\n", pFlow_->rtspUrl_.c_str());
		{
			closestyle_ = 5;// pipeline create fail
		}
	}
	else
	{
		/*loop_ = g_main_loop_new(NULL, FALSE);
		g_main_loop_run(loop_);*/

		bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
		//gst_bus_add_watch(bus_, EncodingTask::onBusMessage, this);
		gst_element_set_state(pipeline_, GST_STATE_PLAYING);
		char* srcElement = NULL;
		do {
			GstMessage* msg = gst_bus_timed_pop_filtered(bus_, 200 * GST_MSECOND, (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

			if (msg != NULL) {
				GError* err;
				gchar* debug_info;

				switch (GST_MESSAGE_TYPE(msg)) {
				case GST_MESSAGE_ERROR:
					gst_message_parse_error(msg, &err, &debug_info);
					g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
					srcElement = GST_OBJECT_NAME(msg->src);

					if (strstr(srcElement, "videoapp_sink") != NULL) {
						closeMessage_ = err->message;
						closestyle_ = -2;
					}
					else if (strstr(srcElement, "filesrc0") != NULL) {
						closeMessage_ = err->message;
						closestyle_ = 2;
					}
				

					if (closeMessage_ == "Unauthorized") {
						closestyle_ = 3;
					}
					g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
					g_clear_error(&err);
					g_free(debug_info);
					terminate = TRUE;
					break;
				case GST_MESSAGE_EOS:
					g_print("End-Of-Stream in rtsp.\n");
					terminate = TRUE;
					break;
				case GST_MESSAGE_STATE_CHANGED:
					/* We are only interested in state-changed messages from the pipeline */
					if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline_)) {
						GstState old_state, new_state, pending_state;
						gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
						/*g_print("Pipeline state changed from %s to %s:\n",
							gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));*/
					}
					break;
				case GST_MESSAGE_REQUEST_STATE: {
					GstState requested_state;
					gst_message_parse_request_state(msg, &requested_state);
					DebugLog::writeLogF("state change to %s was requested by %s\n",
						gst_element_state_get_name(requested_state),
						GST_MESSAGE_SRC_NAME(msg)
					);
					gst_element_set_state(GST_ELEMENT(pipeline_), requested_state);
					break;
				}
				case GST_MESSAGE_LATENCY: {
					printf("redistributing latency\n");
					gst_bin_recalculate_latency(GST_BIN(pipeline_));
					break;
				}

				default:
					/* We should not reach here */
					g_printerr("Unexpected message received.\n");
					break;
				}
				gst_message_unref(msg);
			}
		} while (!terminate);
		/* Free resources */
		gst_object_unref(bus_);
		gst_element_set_state(pipeline_, GST_STATE_NULL);
		gst_object_unref(pipeline_);
	}

	isRun = STATUS_DISCONNECT;

}


void DirectTranscodingTask::myfps(int deltatm) {
	frameCount++;
	unsigned long curtm = ts_time::currentpts(pFlow_->task_start_time_);
	if (lasttm == 0) {
		lasttm = curtm;
		return;
	}

	if (curtm - lasttm > 5000) {

		curfps = frameCount / 5;
		frameCount = 0;
		lasttm = curtm;
	//	printf("######### cur fps = %d\n", curfps);

		if ((curfps - oldfps) >= 3) {
			changeCount++;
		}
		else {
			changeCount = 0;
		}
		if (oldfps > 0 && changeCount >= 3) {
			changeCount = 0;
			closestyle_ = 8;
			//reportStatus();
		}

		if (oldfps == 0) {
			oldfps = curfps;
		}
	}

}

static gboolean before_send_callback(GstElement* rtspsrc, GstRTSPMessage* msg, gpointer udata)
{
	DirectTranscodingTask* pTask = (DirectTranscodingTask*)udata;
	if (pTask->needteardown_ == 1) {
		puts("teardown rtsp gracefully!");
		return false;
	}

	GstRTSPMsgType msgType = gst_rtsp_message_get_type(msg);
	if (msgType == GST_RTSP_MESSAGE_REQUEST) {

		GstRTSPMethod rtspmtd;
		gst_rtsp_message_parse_request(msg, &rtspmtd, NULL, NULL);

		//printf("-------show rtsp methond = %d----\n",rtspmtd);
		if (rtspmtd == GST_RTSP_OPTIONS || rtspmtd == GST_RTSP_ANNOUNCE)
		{
			return false;
		}
	}
	return true;
}

int DirectTranscodingTask::addVideoBranch(int type) {
	printf("-------add video parse------- %d\n", type);
	if (type == 96) {
		demuxer_ = gst_element_factory_make("rtph264depay", ("videodemux" + std::to_string(tid_)).c_str());
		videoparser_ = gst_element_factory_make("h264parse", ("parser" + std::to_string(tid_)).c_str());
	}
	else if (type == 98) {
		demuxer_ = gst_element_factory_make("rtph265depay", ("videodemux" + std::to_string(tid_)).c_str());
		videoparser_ = gst_element_factory_make("h265parse", ("parser" + std::to_string(tid_)).c_str());
	}



	gst_bin_add_many(GST_BIN(pipeline_), demuxer_, videoparser_, NULL);


	if (!gst_element_sync_state_with_parent(demuxer_)) {
		g_printerr("sync audio element stat faile 2.\n");

	}
	if (!gst_element_sync_state_with_parent(videoparser_)) {
		g_printerr("sync audio element stat faile 3.\n");

	}


	if (!gst_element_link_many(demuxer_, videoparser_, NULL)) {
		g_printerr("video demuxer branch Elements could not be linked.\n");
		return 0;
	}

	GstPad* parse_sinkpad = gst_element_get_static_pad(videoparser_, "src");
	GstPad* dec_sinkpad = gst_element_get_static_pad(videodecoder_, "sink");

	if (GST_PAD_LINK_OK != gst_pad_link(parse_sinkpad, dec_sinkpad))
	{
		g_print("rtsp---videodecoder parse pad failed.\n");

	}

	//	gst_element_set_state(pipeline_, GST_STATE_PLAYING);

	printf("-------add video parse------- %d ok\n", type);

	return 0;

}


int DirectTranscodingTask::addAudioBranch(int type)
{
	if (withaudio_ > 0) {

		if (type == 0) {
			audiodemuxer_ = gst_element_factory_make("rtppcmudepay", ("audiodemux" + std::to_string(tid_)).c_str());
			audiodecoder_ = gst_element_factory_make("mulawdec", ("adecode" + std::to_string(tid_)).c_str());
			audioparser_ = gst_element_factory_make("rawaudioparse", ("aparse" + std::to_string(tid_)).c_str());

		}
		else if (type == 8) {
			audiodemuxer_ = gst_element_factory_make("rtppcmadepay", ("audiodemux" + std::to_string(tid_)).c_str());
			audiodecoder_ = gst_element_factory_make("alawdec", ("adecode" + std::to_string(tid_)).c_str());
			audioparser_ = gst_element_factory_make("rawaudioparse", ("aparse" + std::to_string(tid_)).c_str());


		}

		audioencoder_ = gst_element_factory_make("alawenc", ("aencode" + std::to_string(tid_)).c_str());

		aconverter_ = gst_element_factory_make("audioconvert", ("aconve" + std::to_string(tid_)).c_str());
		aconverter1_ = gst_element_factory_make("audioconvert", ("aconvd" + std::to_string(tid_)).c_str());
		queue2_ = gst_element_factory_make("queue", ("queue2" + std::to_string(tid_)).c_str());
		//g_object_set(queue2_, "max-size-buffers", this->pFlow_->queue_size_ * 2, NULL);
		//audioqueue_ = gst_element_factory_make("queue", ("queue_fakesink" + std::to_string(tid_)).c_str());
		if (!audiodemuxer_ || !audiodecoder_ || !audioencoder_) {
			g_printerr("rtsp audio element could not be created.\n");
			return -1;
		}


		g_object_set(audioencoder_, "perfect-timestamp", false, NULL);
		g_object_set(audioencoder_, "perfect-timestamp", false, NULL);
		g_object_set(audioencoder_, "tolerance", 200000000, NULL);


		//gst_base_sink_set_max_lateness(GST_BASE_SINK(appaudiosink_), 70 * GST_MSECOND);
		//gst_base_sink_set_qos_enabled(GST_BASE_SINK(appaudiosink_), TRUE);
		//g_object_set(G_OBJECT(appaudiosink_), "max-buffers", 10, NULL);
		//g_object_set(G_OBJECT(appaudiosink_), "emit-signals", TRUE, NULL);
	/*	gst_pad_add_probe(gst_element_get_static_pad(appaudiosink_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe1, this, NULL);
		g_signal_connect(G_OBJECT(appaudiosink_), "new-sample", G_CALLBACK(DirectTranscodingTask::on_audio_sample), this);*/

		gst_bin_add_many(GST_BIN(pipeline_), audiodemuxer_, audiodecoder_, aconverter_, audioencoder_, queue2_, NULL);

		if (!gst_element_sync_state_with_parent(aconverter_)) {
			g_printerr("sync audio element stat faile 0.\n");

		}
		/*	if (!gst_element_sync_state_with_parent(aconverter1_)) {
				g_printerr("sync audio element stat faile 0.\n");

			}*/
		if (!gst_element_sync_state_with_parent(queue2_)) {
			g_printerr("sync audio element stat faile 2.\n");

		}
		if (!gst_element_sync_state_with_parent(audiodemuxer_)) {
			g_printerr("sync audio element stat faile 2.\n");

		}
		if (!gst_element_sync_state_with_parent(audiodecoder_)) {
			g_printerr("sync audio element stat faile 3.\n");

		}
		/*if (!gst_element_sync_state_with_parent(audioparser_)) {
			g_printerr("sync audio element stat faile 3.\n");

		}*/
		if (!gst_element_sync_state_with_parent(audioencoder_)) {
			g_printerr("sync audio element stat faile 4.\n");

		}

		if (!gst_element_link_many(audiodemuxer_, audiodecoder_, aconverter_, audioencoder_, queue2_, rtsppushsink_, NULL)) {
			g_printerr("audio demuxer branch Elements could not be linked.\n");
			return 0;
		}

		printf("----------audio branch added---- ok\n");

	}

	return 0;

}

int DirectTranscodingTask::initRtspPipeline() {
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	streamsrc_ = gst_element_factory_make("rtspsrc", ("filesrc0" + std::to_string(tid_)).c_str());

	g_object_set(G_OBJECT(streamsrc_), "location", pFlow_->rtspUrl_.c_str(), NULL);
	g_object_set(G_OBJECT(streamsrc_), "latency", pFlow_->delay_input_, NULL);
	g_object_set(G_OBJECT(streamsrc_), "timeout", 3000, NULL);
	g_object_set(G_OBJECT(streamsrc_), "tcp-timeout", 3000, NULL);
	g_object_set(GST_OBJECT(streamsrc_), "debug", true, NULL);
	if (pFlow_->rtspUrl_.find("proto=Onvif") != std::string::npos && pFlow_->rtspUrl_.find("realmonitor") != std::string::npos) {

	}
	else {
			g_object_set(GST_OBJECT(streamsrc_), "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
	}

	g_signal_connect(streamsrc_, "on-sdp", G_CALLBACK(DirectTranscodingTask::on_sdp_callback), this);
	g_signal_connect(streamsrc_, "before-send", G_CALLBACK(before_send_callback), this);
	g_signal_connect(streamsrc_, "pad-added", G_CALLBACK(on_rtsp2dec_link), this);


	gst_bin_add_many(GST_BIN(pipeline_), streamsrc_, NULL);

	return 0;

}

int DirectTranscodingTask::addPipelineDecoder() {
	if (video_pt_ == 96) {
		demuxer_ = gst_element_factory_make("rtph264depay", ("videodemux" + std::to_string(tid_)).c_str());
		videoparser_ = gst_element_factory_make("h264parse", ("parser" + std::to_string(tid_)).c_str());
		videodecoder_ = gst_element_factory_make("mppvideodec", ("videodecode" + std::to_string(tid_)).c_str());

	}
	else if (video_pt_ == 98) {
		demuxer_ = gst_element_factory_make("rtph265depay", ("videodemux" + std::to_string(tid_)).c_str());
		videoparser_ = gst_element_factory_make("h265parse", ("parser" + std::to_string(tid_)).c_str());
		videodecoder_ = gst_element_factory_make("mppvideodec", ("videodecode" + std::to_string(tid_)).c_str());

	}
	else if (video_pt_ == 26) {
		demuxer_ = gst_element_factory_make("rtpjpegdepay", ("videodemux" + std::to_string(tid_)).c_str());
		videoparser_ = gst_element_factory_make("jpegparse", ("parser" + std::to_string(tid_)).c_str());
		videodecoder_ = gst_element_factory_make("mppjpegdec", ("videodecode" + std::to_string(tid_)).c_str());

	}

	gst_bin_add_many(GST_BIN(pipeline_), demuxer_, videoparser_, videodecoder_, NULL);

	if (!gst_element_sync_state_with_parent(demuxer_)) {
		g_printerr("sync audio element stat faile 2.\n");

	}
	if (!gst_element_sync_state_with_parent(videoparser_)) {
		g_printerr("sync audio element stat faile 3.\n");

	}

	if (!gst_element_sync_state_with_parent(videodecoder_)) {
		g_printerr("sync audio element stat faile 2.\n");

	}

	//gst_pad_add_probe(gst_element_get_static_pad(videodecoder_, "src"), GST_PAD_PROBE_TYPE_BUFFER, pad_probe, this, NULL);
	//gst_pad_add_probe(gst_element_get_static_pad(videodecoder_, "src"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe, this, NULL);

	gst_pad_add_probe(gst_element_get_static_pad(videodecoder_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe2, this, NULL);

	gst_pad_add_probe(gst_element_get_static_pad(videodecoder_, "src"), GST_PAD_PROBE_TYPE_BUFFER, pad_probefps, this, NULL);


	

	if (withaudio_ > 0) {

		if (audio_pt_ == 0) {
			audiodemuxer_ = gst_element_factory_make("rtppcmudepay", ("audiodemux" + std::to_string(tid_)).c_str());
			audiodecoder_ = gst_element_factory_make("mulawdec", ("adecode" + std::to_string(tid_)).c_str());
			//	audioparser_ = gst_element_factory_make("rawaudioparse", ("aparse" + std::to_string(tid_)).c_str());

		}
		else if (audio_pt_ == 8) {
			audiodemuxer_ = gst_element_factory_make("rtppcmadepay", ("audiodemux" + std::to_string(tid_)).c_str());
			audiodecoder_ = gst_element_factory_make("alawdec", ("adecode" + std::to_string(tid_)).c_str());
			//	audioparser_ = gst_element_factory_make("rawaudioparse", ("aparse" + std::to_string(tid_)).c_str());
		}
		else if (audio_pt_ == 21) {
			audiodemuxer_ = gst_element_factory_make("rtpg726depay", ("audiodemux" + std::to_string(tid_)).c_str());
			audiodecoder_ = gst_element_factory_make("avdec_g726", ("adecode" + std::to_string(tid_)).c_str());
			//	audioparser_ = gst_element_factory_make("rawaudioparse", ("aparse" + std::to_string(tid_)).c_str());
		}
		else if (audio_pt_ == 101) {
			audiodemuxer_ = gst_element_factory_make("rtpopusdepay", ("audiodemux" + std::to_string(tid_)).c_str());
			audiodecoder_ = gst_element_factory_make("opusdec", ("adecode" + std::to_string(tid_)).c_str());
			//	audioparser_ = gst_element_factory_make("rawaudioparse", ("aparse" + std::to_string(tid_)).c_str());
		}


		gst_bin_add_many(GST_BIN(pipeline_), audiodemuxer_, audiodecoder_, NULL);

		if (!gst_element_sync_state_with_parent(audiodemuxer_)) {
			g_printerr("sync audio element stat faile 2.\n");

		}
		if (!gst_element_sync_state_with_parent(audiodecoder_)) {
			g_printerr("sync audio element stat faile 3.\n");

		}
		printf("----------audio branch added---- ok\n");

	}

}
int DirectTranscodingTask::addPipelineRtsp() {
	selectVideoEncoder();
	videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	rtsppushsink_ = gst_element_factory_make("rtspclientsink", ("videoapp_sink" + std::to_string(tid_)).c_str());
	videorate_ = gst_element_factory_make("videorate", ("videorate0" + std::to_string(tid_)).c_str());
	cpasfilter_ = gst_element_factory_make("capsfilter" , ("videoFilter0" + std::to_string(tid_)).c_str());
	/*GstCaps* caps = gst_caps_new_simple("video/x-raw", \
		"framerate", GST_TYPE_FRACTION, 25, 1, \
		NULL);*/

	GstCaps* caps = gst_caps_new_simple("video/x-raw", \
		"format", G_TYPE_STRING, "NV12", \
		"framerate", GST_TYPE_FRACTION, 25, 1, \
		NULL);

	g_object_set(cpasfilter_, "caps", caps,  NULL);
	gst_caps_unref(caps);

	


	if (!pipeline_ | !videodecoder_ || !videoqueue_ || !rtsppushsink_) {
		g_printerr("One element could not be created 2.\n");
		return -1;
	}
	puts("=====================add video rate===============");
	// appsink setting
	g_object_set(G_OBJECT(rtsppushsink_), "location", pFlow_->outputpath_.c_str(), NULL);

	gst_bin_add_many(GST_BIN(pipeline_), vconverter_, videorate_,cpasfilter_, videoencoder_, videoqueue_, rtsppushsink_, NULL);

	if (!gst_element_sync_state_with_parent(vconverter_)) {
		g_printerr("sync audio element stat faile 3.\n");

	}
	if (!gst_element_sync_state_with_parent(cpasfilter_)) {
		g_printerr("sync audio element stat faile 3.\n");

	}
	if (!gst_element_sync_state_with_parent(videorate_)) {
		g_printerr("sync audio element stat faile 3.\n");

	}

	if (!gst_element_sync_state_with_parent(videoencoder_)) {
		g_printerr("sync audio element stat faile 2.\n");

	}
	if (!gst_element_sync_state_with_parent(videoqueue_)) {
		g_printerr("sync audio element stat faile 3.\n");

	}
	if (!gst_element_sync_state_with_parent(rtsppushsink_)) {
		g_printerr("sync audio element stat faile 3.\n");

	}

	if (!gst_element_link_many(demuxer_, videoparser_, videodecoder_, vconverter_, videorate_, cpasfilter_,  videoencoder_, videoqueue_, rtsppushsink_, NULL)) {
		g_printerr("appsink Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	//gst_pad_add_probe(gst_element_get_static_pad(videoencoder_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe, this, NULL);

	if (withaudio_ > 0) {

		audioencoder_ = gst_element_factory_make("alawenc", ("aencode" + std::to_string(tid_)).c_str());
		aconverter_ = gst_element_factory_make("audioconvert", ("aconve" + std::to_string(tid_)).c_str());
		queue2_ = gst_element_factory_make("queue", ("queue2" + std::to_string(tid_)).c_str());


		if (!audiodemuxer_ || !audiodecoder_ || !audioencoder_) {
			g_printerr("rtsp audio element could not be created.\n");
			return -1;
		}


		g_object_set(audioencoder_, "perfect-timestamp", false, NULL);
		g_object_set(audioencoder_, "perfect-timestamp", false, NULL);
		g_object_set(audioencoder_, "tolerance", 200000000, NULL);


		gst_bin_add_many(GST_BIN(pipeline_), aconverter_, audioencoder_, queue2_, NULL);

		if (!gst_element_sync_state_with_parent(aconverter_)) {
			g_printerr("sync audio element stat faile 0.\n");

		}

		if (!gst_element_sync_state_with_parent(queue2_)) {
			g_printerr("sync audio element stat faile 2.\n");

		}

		if (!gst_element_sync_state_with_parent(audioencoder_)) {
			g_printerr("sync audio element stat faile 4.\n");

		}

		if (!gst_element_link_many(audiodemuxer_, audiodecoder_, aconverter_, audioencoder_, queue2_, rtsppushsink_, NULL)) {
			g_printerr("audio demuxer branch Elements could not be linked.\n");
			return 0;
		}
	}


}
int DirectTranscodingTask::addPipelineRtmp() {
	selectVideoEncoder();
	streamsink_ = gst_element_factory_make("rtmp2sink", ("sink" + std::to_string(tid_)).c_str());
	avmuxer_ = gst_element_factory_make("flvmux", ("muxer" + std::to_string(tid_)).c_str());

	if (pFlow_->encodetype_ == "h264") {
		videoparser_ = gst_element_factory_make("h264parse", ("parser" + std::to_string(tid_)).c_str());
	}
	else if (pFlow_->encodetype_ == "h265") {
		videoparser_ = gst_element_factory_make("h265parse", ("parser" + std::to_string(tid_)).c_str());
	}
	if (!pipeline_  || !videoencoder_ || !videoparser_ || !avmuxer_ || !streamsink_)
	{
		g_printerr("Not all elements could be created.\n");
		return -1;
	}


	//puts(pFlow_->outputpath_.c_str());
	EdUrlParser* url = EdUrlParser::parseUrl(pFlow_->outputpath_);
	vector<string> paths;
	EdUrlParser::parsePath(&paths, url->path);
	for (int i = 0; i < paths.size(); i++) {
		DebugLog::writeLogF("path part: %d %s \n", i, paths[i].c_str());
	}
	//printf("host=%s\n port=%d\n app=%s\n\n", url->hostName.c_str(), 1935,url->path.c_str());

	g_object_set(streamsink_, "location", pFlow_->outputpath_.c_str(), NULL);
	g_object_set(streamsink_, "host", url->hostName.c_str(), NULL);
	g_object_set(streamsink_, "port", 1935, NULL);


	if (url != NULL) // ==> make sure to free url object allocated by EdUrlParser
		delete url;

	puts(pFlow_->outputpath_.c_str());
	//	g_object_set(streamsink_, "location", pFlow_->outputpath_.c_str(), NULL);
		//g_object_set(streamsink_, "debug", true, NULL);

	gst_bin_add_many(GST_BIN(pipeline_), videoencoder_, videoparser_, avmuxer_, streamsink_, NULL);

	if (!gst_element_link_many( videoencoder_, videoparser_, avmuxer_, streamsink_, NULL)) {
		g_printerr("rtmp link error. \n");

		return -1;
	}
}
int DirectTranscodingTask::addPipelineFile() {
	selectVideoEncoder();
	if (pFlow_->encodetype_ == "h264") {
		videoparserout_ = gst_element_factory_make("h264parse", ("parserout" + std::to_string(tid_)).c_str());
	}
	else if (pFlow_->encodetype_ == "h265") {
		videoparserout_ = gst_element_factory_make("h265parse", ("parserout" + std::to_string(tid_)).c_str());
	}

	string FileSuffix;
	if (pFlow_->outputtype_ == "mp4") {
		avmuxer_ = gst_element_factory_make("qtmux", ("muxer" + std::to_string(tid_)).c_str());
		FileSuffix = "mp4";
	}
	else if (pFlow_->outputtype_ == "flv") {
		avmuxer_ = gst_element_factory_make("flvmux", ("muxer" + std::to_string(tid_)).c_str());
		FileSuffix = "flv";
	}
	else if (pFlow_->outputtype_ == "ts") {
		avmuxer_ = gst_element_factory_make("mpegtsmux", ("muxer" + std::to_string(tid_)).c_str());
		FileSuffix = "ts";
	}
	streamsink_ = gst_element_factory_make("filesink", ("sink" + std::to_string(tid_)).c_str());

	if (!pipeline_ || !videoencoder_ || !videoparserout_ || !avmuxer_ || !streamsink_)
	{
		g_printerr("Not all elements could be created.\n");
		return -1;
	}
	g_object_set(avmuxer_, "faststart", TRUE, NULL);
	g_object_set(avmuxer_, "streamable", TRUE, NULL);
	g_object_set(avmuxer_, "presentation-time", true, NULL);
	g_object_set(avmuxer_, "fragment-duration", 1000, NULL);

	queue1_ = gst_element_factory_make("queue", ("queue1" + std::to_string(tid_)).c_str());
	g_object_set(queue1_, "max-size-buffers", this->pFlow_->buf_output_, NULL);


	char dstFilename[256] = { 0 };
	//sprintf(dstFilename, "%s/%s_%s_%d.%s", pFlow_->outputpath_.c_str(), pFlow_->taskid_.c_str(), pFlow_->encodetype_.c_str(), time(NULL), FileSuffix.c_str());
	sprintf(dstFilename, "%s", pFlow_->outputpath_.c_str());
	g_object_set(streamsink_, "location", dstFilename, NULL);
	g_object_set(streamsink_, "o-sync", TRUE, NULL);


	gst_bin_add_many(GST_BIN(pipeline_), vconverter_, videoencoder_, queue1_, videoparserout_, avmuxer_, streamsink_, NULL);

	if (!gst_element_sync_state_with_parent(vconverter_)) {
		g_printerr("sync audio element stat faile in file 1.\n");

	}
	if (!gst_element_sync_state_with_parent(videoencoder_)) {
		g_printerr("sync audio element stat faile in file 2.\n");

	}
	if (!gst_element_sync_state_with_parent(queue1_)) {
		g_printerr("sync audio element stat faile in file 3.\n");

	}
	if (!gst_element_sync_state_with_parent(videoparserout_)) {
		g_printerr("sync audio element stat faile in file 4.\n");

	}
	if (!gst_element_sync_state_with_parent(avmuxer_)) {
		g_printerr("sync audio element stat faile in file 5.\n");

	}
	if (!gst_element_sync_state_with_parent(streamsink_)) {
		g_printerr("sync audio element stat faile in file 6.\n");

	}

	if (!gst_element_link_many(demuxer_, videoparser_, videodecoder_, vconverter_, videoencoder_, queue1_, videoparserout_, NULL) || !gst_element_link_many(avmuxer_, streamsink_, NULL)) {
		return -1;
	}

	puts("check video-----------------------------");
	GstPadTemplate* mux_src_pad_template;
	if (pFlow_->outputtype_ == "ts") {
		mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(avmuxer_), "sink_%d");
	}
	else {
		mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(avmuxer_), "video_%u");
	}
	if (mux_src_pad_template == NULL)		g_printerr("mux_src_pad_template null. \n");

	GstPad* muxer_pad = gst_element_request_pad(avmuxer_, mux_src_pad_template, NULL, NULL);
	if (muxer_pad == NULL)		g_printerr("muxer_pad null. \n");

	GstPad* parser_pad = gst_element_get_static_pad(videoparserout_, "src");
	if (parser_pad == NULL)		g_printerr("parser_pad null. \n");

	if (gst_pad_link(parser_pad, muxer_pad) != GST_PAD_LINK_OK) {
		g_printerr("video parser-muxer link error. \n");
		return -1;
	}
	gst_object_unref(parser_pad);
	gst_object_unref(muxer_pad);

	puts("check video-----------------------------1");

	if (withaudio_ > 0) {
		puts("add audio-----------------------0");

		aconverter_ = gst_element_factory_make("audioconvert", ("aconv" + std::to_string(tid_)).c_str());
		audioencoder_ = gst_element_factory_make("voaacenc", ("audioencoder" + std::to_string(tid_)).c_str());
		if (!audioencoder_) {
			audioencoder_ = gst_element_factory_make("avenc_aac", ("audioencoder" + std::to_string(tid_)).c_str());
		}
		queue2_ = gst_element_factory_make("queue", ("queue2" + std::to_string(tid_)).c_str());
		g_object_set(queue2_, "max-size-buffers", this->pFlow_->buf_output_ * 2, NULL);
		audioparser_ = gst_element_factory_make("aacparse", ("audioparser" + std::to_string(tid_)).c_str());

		if (!audioencoder_ || !audioparser_) {
			DebugLog::writeLogF("audio element create failed 1\n");
			return -1;
		}


		setExtraParameter(audioencoder_, pFlow_->aac_extra_);


		g_object_set(audioencoder_, "perfect-timestamp", false, NULL);
		g_object_set(audioencoder_, "tolerance", 200000000, NULL);


		gst_bin_add_many(GST_BIN(pipeline_), aconverter_, audioencoder_, queue2_, audioparser_, NULL);
		if (!gst_element_sync_state_with_parent(aconverter_)) {
			g_printerr("sync audio element stat faile 3.\n");

		}
		if (!gst_element_sync_state_with_parent(audioencoder_)) {
			g_printerr("sync audio element stat faile 3.\n");

		}
		if (!gst_element_sync_state_with_parent(queue2_)) {
			g_printerr("sync audio element stat faile 3.\n");

		}
		if (!gst_element_sync_state_with_parent(audioparser_)) {
			g_printerr("sync audio element stat faile 3.\n");

		}
		if (!gst_element_link_many(audiodemuxer_, audiodecoder_, aconverter_, audioencoder_, queue2_, audioparser_, NULL)) {
			g_printerr("audio branch link error. \n");
			return -1;
		}


		GstPadTemplate* mux_src_pad_template;
		if (pFlow_->outputtype_ != "ts") {
			mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(avmuxer_), "audio_%u");
			if (mux_src_pad_template == NULL)		g_printerr("mux_src_pad_template null. \n");

			GstPad* muxer_pad = gst_element_request_pad(avmuxer_, mux_src_pad_template, NULL, NULL);
			if (muxer_pad == NULL)		g_printerr("muxer_pad null. \n");

			GstPad* parser_pad = gst_element_get_static_pad(audioparser_, "src");
			if (parser_pad == NULL)		g_printerr("parser_pad null. \n");

			if (gst_pad_link(parser_pad, muxer_pad) != GST_PAD_LINK_OK) {
				g_printerr("audio parse-mux link error. \n");
				return -1;
			}
			gst_object_unref(parser_pad);
			gst_object_unref(muxer_pad);
		}
		else {
			mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(avmuxer_), "sink_%d");
			if (mux_src_pad_template == NULL)		g_printerr("mux_src_pad_template null. \n");

			GstPad* muxer_pad = gst_element_request_pad(avmuxer_, mux_src_pad_template, NULL, NULL);
			if (muxer_pad == NULL)		g_printerr("muxer_pad null. \n");
			GstPad* parser_pad = gst_element_get_static_pad(audioparser_, "src");
			if (parser_pad == NULL)		g_printerr("parser_pad null. \n");
			if (gst_pad_link(parser_pad, muxer_pad) != GST_PAD_LINK_OK) {
				g_printerr("audio parse-mux link error. \n");
				return -1;
			}
			gst_object_unref(parser_pad);
			gst_object_unref(muxer_pad);
		}
	}

}
int DirectTranscodingTask::addPipelineRtp() {
	selectVideoEncoder();
	videoparser_ = gst_element_factory_make("rtph264pay", ("rtph264pay" + std::to_string(tid_)).c_str());
	avmuxer_ = gst_element_factory_make("rtpbin", ("rtpbin" + std::to_string(tid_)).c_str());
	streamsink_ = gst_element_factory_make("udpsink", ("udpsink" + std::to_string(tid_)).c_str()); // rtpbin + udpsink | tcpclientsink tcpclientsink
	if (!streamsink_) {
		g_printerr("rtpclientsink could be created.\n");
	}
	if (!pipeline_ || !vconverter_  || !videoencoder_ || !videoparser_ || !streamsink_)
	{
		g_printerr("Not all elements could be created.\n");
	}
	puts(pFlow_->outputpath_.c_str());
	g_object_set(GST_OBJECT(streamsink_), "host", pFlow_->outputpath_.c_str(), NULL);
	g_object_set(GST_OBJECT(streamsink_), "port", 8000, NULL);
	
	GstPadTemplate* mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(avmuxer_), "send_rtp_sink_%u");
	if (mux_src_pad_template == NULL)		g_printerr("recv_rtp_sink_pad_template null. \n");
	GstPad* muxer_pad = gst_element_request_pad(avmuxer_, mux_src_pad_template, NULL, NULL);
	if (muxer_pad == NULL)		g_printerr("muxer_pad null. \n");
	gst_object_unref(muxer_pad);


	gst_bin_add_many(GST_BIN(pipeline_),  vconverter_, videoencoder_, videoparser_, avmuxer_, streamsink_, NULL);

	if (!gst_element_sync_state_with_parent(vconverter_)) {
		g_printerr("sync audio element stat faile in file 1.\n");

	}
	if (!gst_element_sync_state_with_parent(videoencoder_)) {
		g_printerr("sync audio element stat faile in file 1.\n");

	}
	if (!gst_element_sync_state_with_parent(videoparser_)) {
		g_printerr("sync audio element stat faile in file 1.\n");

	}
	if (!gst_element_sync_state_with_parent(avmuxer_)) {
		g_printerr("sync audio element stat faile in file 1.\n");

	}
	if (!gst_element_sync_state_with_parent(streamsink_)) {
		g_printerr("sync audio element stat faile in file 1.\n");

	}
	if (!gst_element_link_many(demuxer_, videoparser_, videodecoder_, vconverter_, videoencoder_, videoparser_, avmuxer_, streamsink_, NULL)) {
		puts("================link error");
		return -1;
	}

	if (withaudio_ > 0) {
		aconverter_ = gst_element_factory_make("audioconvert", ("aconv" + std::to_string(tid_)).c_str());
		queue2_ = gst_element_factory_make("queue", ("queue2" + std::to_string(tid_)).c_str());
		g_object_set(queue2_, "max-size-buffers", this->pFlow_->buf_output_ * 2, NULL);
		audioencoder_ = gst_element_factory_make("alawenc", ("audioencoder" + std::to_string(tid_)).c_str());
		audioparser_ = gst_element_factory_make("rtppcmapay", ("rtppcmapay" + std::to_string(tid_)).c_str());
		streamsink1_ = gst_element_factory_make("udpsink", ("udpsink_a" + std::to_string(tid_)).c_str()); // rtpbin + udpsink | tcpclientsink tcpclientsink
		if ( !audioencoder_ || !audioparser_) {
			DebugLog::writeLogF("audio element create failed\n");
			return -1;
		}
		g_object_set(GST_OBJECT(streamsink1_), "host", pFlow_->outputpath_.c_str(), NULL);
		g_object_set(GST_OBJECT(streamsink1_), "port", 8001, NULL);
		setExtraParameter(audioencoder_, pFlow_->aac_extra_);
	

		g_object_set(audioencoder_, "perfect-timestamp", false, NULL);
		GstPadTemplate* mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(avmuxer_), "send_rtp_sink_%u");
		if (mux_src_pad_template == NULL)		g_printerr("recv_rtp_sink_pad_template null in audio. \n");
		GstPad* muxer_pad = gst_element_request_pad(avmuxer_, mux_src_pad_template, NULL, NULL);
		if (muxer_pad == NULL)		g_printerr("muxer_pad null in audio. \n");
		gst_object_unref(muxer_pad);

		gst_bin_add_many(GST_BIN(pipeline_),  aconverter_, queue2_, audioencoder_, audioparser_, streamsink1_, NULL);
		if (!gst_element_sync_state_with_parent(aconverter_)) {
			g_printerr("sync audio element stat faile in file 1.\n");

		}
		if (!gst_element_sync_state_with_parent(queue2_)) {
			g_printerr("sync audio element stat faile in file 1.\n");

		}
		if (!gst_element_sync_state_with_parent(audioencoder_)) {
			g_printerr("sync audio element stat faile in file 1.\n");

		}
		if (!gst_element_sync_state_with_parent(audioparser_)) {
			g_printerr("sync audio element stat faile in file 1.\n");

		}
		if (!gst_element_sync_state_with_parent(streamsink1_)) {
			g_printerr("sync audio element stat faile in file 1.\n");

		}
		if (!gst_element_link_many(audiodemuxer_, audiodecoder_, aconverter_, queue2_, audioencoder_, audioparser_, avmuxer_, streamsink1_, NULL)) {
			g_printerr("audio branch link error. \n");
			return -1;
		}
	}

	return 0;
}

int DirectTranscodingTask::completeRtspPipeline() {
	// Build Pipeline 
	puts("==================  complete pipe line===============");
	videodecoder_ = gst_element_factory_make("mppvideodec", ("videodecode" + std::to_string(tid_)).c_str());
	selectVideoEncoder();
	videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	rtsppushsink_ = gst_element_factory_make("rtspclientsink", ("videoapp_sink" + std::to_string(tid_)).c_str());


	if (!pipeline_ | !videodecoder_ || !videoqueue_ || !rtsppushsink_) {
		g_printerr("One element could not be created 2.\n");
		return -1;
	}

	// appsink setting
	g_object_set(G_OBJECT(rtsppushsink_), "location", pFlow_->outputpath_.c_str(), NULL);

	if (video_pt_ == 96) {
		demuxer_ = gst_element_factory_make("rtph264depay", ("videodemux" + std::to_string(tid_)).c_str());
		videoparser_ = gst_element_factory_make("h264parse", ("parser" + std::to_string(tid_)).c_str());
	}
	else if (video_pt_ == 98) {
		demuxer_ = gst_element_factory_make("rtph265depay", ("videodemux" + std::to_string(tid_)).c_str());
		videoparser_ = gst_element_factory_make("h265parse", ("parser" + std::to_string(tid_)).c_str());
	}



	gst_bin_add_many(GST_BIN(pipeline_), demuxer_, videoparser_, videodecoder_, vconverter_, videoencoder_, videoqueue_, rtsppushsink_, NULL);

	if (!gst_element_sync_state_with_parent(demuxer_)) {
		g_printerr("sync audio element stat faile 2.\n");

	}
	if (!gst_element_sync_state_with_parent(videoparser_)) {
		g_printerr("sync audio element stat faile 3.\n");

	}

	if (!gst_element_sync_state_with_parent(videodecoder_)) {
		g_printerr("sync audio element stat faile 2.\n");

	}
	if (!gst_element_sync_state_with_parent(vconverter_)) {
		g_printerr("sync audio element stat faile 3.\n");

	}

	if (!gst_element_sync_state_with_parent(videoencoder_)) {
		g_printerr("sync audio element stat faile 2.\n");

	}
	if (!gst_element_sync_state_with_parent(videoqueue_)) {
		g_printerr("sync audio element stat faile 3.\n");

	}
	if (!gst_element_sync_state_with_parent(rtsppushsink_)) {
		g_printerr("sync audio element stat faile 3.\n");

	}

	if (!gst_element_link_many(demuxer_, videoparser_, videodecoder_, vconverter_, videoencoder_, videoqueue_, rtsppushsink_, NULL)) {
		g_printerr("appsink Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	
	//gst_pad_add_probe(gst_element_get_static_pad(streamsrc_, "src"), GST_PAD_PROBE_TYPE_BUFFER, pad_probe, this, NULL);
	//gst_pad_add_probe(gst_element_get_static_pad(videodecoder_, "sink_%u"), GST_PAD_PROBE_TYPE_BUFFER, appsink_query_cb, NULL, NULL);
	addAudioBranch(audio_pt_);

	//addVideoBranch(video_pt_);

	return 0;
}

GstPadProbeReturn DirectTranscodingTask::appsink_query_cb(GstPad* pad G_GNUC_UNUSED, GstPadProbeInfo* info, gpointer user_data G_GNUC_UNUSED)
{
	GstQuery* query = (GstQuery*)info->data;

	puts(">>>>>>>>>>>>>>>> video appsink_query_cb\n\n\n");
	if (GST_QUERY_TYPE(query) != GST_QUERY_ALLOCATION)
		return GST_PAD_PROBE_OK;

	gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);

	return GST_PAD_PROBE_HANDLED;
}

void DirectTranscodingTask::on_rtsp2dec_link(GstElement* element, GstPad* pad, gpointer data) {

	DirectTranscodingTask* pTask = (DirectTranscodingTask*)data;

	/* Retrieve negotiated caps (or acceptable caps if negotiation is not finished yet) */

	string tmpstr = GST_PAD_NAME(pad);
	int pos = tmpstr.rfind('_');
	tmpstr = tmpstr.substr(pos + 1);
	int payloadtype = atoi(tmpstr.c_str());
	int mediatype = GstUtil::checkMediaPtype(pad);

	g_print("----decodebin----Received new pad '%s' from '%s' payload=%d mediatype=%d\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(element), payloadtype, mediatype);


	//if (mediatype !=1 &&(mediatype == 0 || payloadtype == 96 || payloadtype == 97 || payloadtype == 98)) {
	if (mediatype != 1 && (mediatype == 0 || payloadtype == 96 || payloadtype == 97 || payloadtype == 98)) {


		//pTask->addVideoBranch(payloadtype);
		GstPad* decode_sinkpad = gst_element_get_static_pad(pTask->demuxer_, "sink");
		//GstPad* decode_sinkpad = gst_element_get_static_pad(pTask->videodecoder_, "sink");

		if (gst_pad_is_linked(decode_sinkpad)) {
			g_print("video rtsp-decoder already linked. Ignoring.\n");
			gst_object_unref(decode_sinkpad);
			return;

		}

		pTask->is_video_added = 1;
		if (GST_PAD_LINK_OK != gst_pad_link(pad, decode_sinkpad))
		{
			g_print("rtsp---videodecoder failed.\n");

		}


		puts("----link---- video ok ");


		//	gst_pad_add_probe(decode_sinkpad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)cb_have_data, data, NULL);
		//gst_object_unref(decode_sinkpad);
	}
	else if (payloadtype == 107) {
		pTask->fakesink_ = gst_element_factory_make("fakesink", ("appdatasink_" + std::to_string(pTask->tid_)).c_str());
		gst_bin_add_many(GST_BIN(pTask->pipeline_), pTask->fakesink_, NULL);
		if (!gst_element_sync_state_with_parent(pTask->fakesink_))
		{
			g_printerr("sync fake element stat faile 1.\n");
		}
		GstPad* decode_sinkpad = gst_element_get_static_pad(pTask->fakesink_, "sink");
		if (gst_pad_is_linked(decode_sinkpad)) {
			g_print("app rtsp-decoder already linked. Ignoring.\n");
			gst_object_unref(decode_sinkpad);
			return;
		}
		if (GST_PAD_LINK_OK != gst_pad_link(pad, decode_sinkpad))
		{
			g_print("rtsp---app data sink failed.\n");
		}
	}
	else if (mediatype == 1) {
		if (pTask->withaudio_ <= 0 || pTask->is_audio_added == 1)
			return;
		pTask->is_audio_added = 1;
		//	pTask->addAudioBranch(payloadtype);

		puts("----link---- audio");


		GstPad* decode_sinkpad = gst_element_get_static_pad(pTask->audiodemuxer_, "sink");
		if (gst_pad_is_linked(decode_sinkpad)) {
			g_print("We are already linked. Ignoring.\n");
			gst_object_unref(decode_sinkpad);
			return;

		}

		if (GST_PAD_LINK_OK != gst_pad_link(pad, decode_sinkpad))
		{
			g_print("pad link :rtsp---audiodecoder failed.\n");

		}
		puts("----link---- audio---ok");

		gst_object_unref(decode_sinkpad);
	}
}

int DirectTranscodingTask::selectVideoEncoder()
{
	if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV) {
		vconverter_ = gst_element_factory_make("nvvideoconvert", ("vconv" + std::to_string(tid_)).c_str());
	}
	else {
		vconverter_ = gst_element_factory_make("videoconvert", ("vconv" + std::to_string(tid_)).c_str());
	}

	if (pFlow_->encodetype_ == "h264") {
		if (TaskManager::GetInstance()->chip_type_ == CHIP_ROCKCHIP) {

			videoencoder_ = gst_element_factory_make("mpph264enc", ("encoder" + std::to_string(tid_)).c_str());

			// 	g_object_set(G_OBJECT(videoencoder_), "qos", 1, NULL);
				//	g_object_set(G_OBJECT(videoencoder_), "qp-init", 30, NULL);
				//	g_object_set(G_OBJECT(videoencoder_), "qp-max", 35, NULL);
				//	g_object_set(G_OBJECT(videoencoder_), "qp-min", 15, NULL);

			g_object_set(G_OBJECT(videoencoder_), "header-mode", 1, NULL);
			g_object_set(G_OBJECT(videoencoder_), "max-reenc", 0, NULL);
			g_object_set(G_OBJECT(videoencoder_), "min-force-key-unit-interval", 0, NULL);
			g_object_set(G_OBJECT(videoencoder_), "sei-mode", 1, NULL);


			//	printf("===========mpp 264========set bps================= %d\n", pFlow_->bitrate_ * 1024);
			g_object_set(G_OBJECT(videoencoder_), "bps", pFlow_->bitrate_ * 1024 , NULL);
			g_object_set(G_OBJECT(videoencoder_), "bps-max", pFlow_->bitrate_ * 1024 * 2, NULL);
			g_object_set(G_OBJECT(videoencoder_), "gop", pFlow_->gop_, NULL);
			//g_object_set(G_OBJECT(videoencoder_), "profile", pFlow_->profile_, NULL);
			g_object_set(G_OBJECT(videoencoder_), "rc-mode", pFlow_->rcmode_, NULL);
			g_object_set(G_OBJECT(videoencoder_), "header-mode", 1, NULL);

			

		/*	guint curbps = 0;
			g_object_get(G_OBJECT(videoencoder_), "bps", &curbps, NULL);
			guint mxbpx = 0;
			g_object_get(G_OBJECT(videoencoder_), "bps-max", &mxbpx, NULL);*/


			//	printf("==================mpp 264 read============= bps=%d  max=%d\n", curbps, mxbpx);

		}
		else if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV && pFlow_->enc_sel_ != 0) {
			videoencoder_ = gst_element_factory_make("nvh264enc", ("encoder" + std::to_string(tid_)).c_str());//nvv4l2h264enc nvh264enc
			puts("------nvh264enc ---------------------");
			if (!videoencoder_) {
				puts("nvv encoder creating failed");
				return -1;
			}
		}
		else if (TaskManager::GetInstance()->chip_type_ == CHIP_SOFTWARE || TaskManager::GetInstance()->chip_type_ == CHIP_TRYCPU || pFlow_->enc_sel_ == 0) {
			videoencoder_ = gst_element_factory_make("x264enc", ("encoder13" + std::to_string(tid_)).c_str());
			if (!videoencoder_) {
				videoencoder_ = gst_element_factory_make("openh264enc", ("encoder" + std::to_string(tid_)).c_str());
				if (!videoencoder_) {
					videoencoder_ = gst_element_factory_make("avenc_h264_omx", ("encoder" + std::to_string(tid_)).c_str());
				}
			}
			g_object_set(G_OBJECT(videoencoder_), "bitrate", pFlow_->bitrate_, NULL);
			g_object_set(G_OBJECT(videoencoder_), "pass", pFlow_->rcmode_ == 0 ? 17 : 0, NULL);

		}

		TSJson::Value::Members mem = pFlow_->x264_extra_.getMemberNames();
		for (auto iter = mem.begin(); iter != mem.end(); iter++)
		{
			const char* paramname = (*iter).c_str();
				//printf("set %s: type:%d val:", (char*)paramname, pFlow_->x264_extra_[*iter].type());
			if (pFlow_->x264_extra_[*iter].type() == TSJson::stringValue)
			{
				const char* strVal = pFlow_->x264_extra_[*iter].asString().c_str();
				//	printf("%s \n", (char*)strVal);
				g_object_set(G_OBJECT(videoencoder_), paramname, strVal, NULL);

			}
			else if (pFlow_->x264_extra_[*iter].type() == TSJson::realValue)
			{
				float fVal = pFlow_->x264_extra_[*iter].asDouble();
				//		printf("%f \n", fVal);
				g_object_set(G_OBJECT(videoencoder_), paramname, fVal, NULL);
			}
			else if (pFlow_->x264_extra_[*iter].type() == TSJson::uintValue)
			{
				int iVal = pFlow_->x264_extra_[*iter].asUInt();
						//printf(" val uint %d \n", pFlow_->x264_extra_[*iter].asUInt());
						//if (strstr(paramname, "bps"))iVal = 1024 * 1024;
				g_object_set(G_OBJECT(videoencoder_), paramname, iVal, NULL);
			}
			else if (pFlow_->x264_extra_[*iter].type() == TSJson::intValue)
			{
				int iVal = pFlow_->x264_extra_[*iter].asInt();
						//printf(" val int %d \n", pFlow_->x264_extra_[*iter].asInt());
				g_object_set(G_OBJECT(videoencoder_), paramname, iVal, NULL);
			}
			else if (pFlow_->x264_extra_[*iter].type() == TSJson::booleanValue)
			{

				int iVal = pFlow_->x264_extra_[*iter].asBool();
				//		printf("%d \n", pFlow_->x264_extra_[*iter].asUInt());
				g_object_set(G_OBJECT(videoencoder_), paramname, iVal, NULL);
			}

		}
	}
	else if (pFlow_->encodetype_ == "h265") {
		puts("---------------- 265 encoder create ----------------------");
		if (TaskManager::GetInstance()->chip_type_ == CHIP_ROCKCHIP) {
			videoencoder_ = gst_element_factory_make("mpph265enc", ("encoder" + std::to_string(tid_)).c_str());
			//	printf("===========mpp 265========set bps================= %d\n", pFlow_->bitrate_ * 1024);
			g_object_set(G_OBJECT(videoencoder_), "header-mode", 1, NULL);
			g_object_set(G_OBJECT(videoencoder_), "max-reenc", 0, NULL);
			g_object_set(G_OBJECT(videoencoder_), "min-force-key-unit-interval", 0, NULL);
			g_object_set(G_OBJECT(videoencoder_), "sei-mode", 1, NULL);

			g_object_set(G_OBJECT(videoencoder_), "bps", pFlow_->bitrate_ * 1024, NULL);
			g_object_set(G_OBJECT(videoencoder_), "bps-max", pFlow_->bitrate_ * 1024, NULL);

			g_object_set(G_OBJECT(videoencoder_), "gop", pFlow_->gop_, NULL);
			g_object_set(G_OBJECT(videoencoder_), "rc-mode", pFlow_->rcmode_, NULL);
			//	g_object_set(G_OBJECT(videoencoder_), "qos", 1, NULL);
			//	g_object_set(G_OBJECT(videoencoder_), "qp-init", 30, NULL);
			//	g_object_set(G_OBJECT(videoencoder_), "qp-max", 35, NULL);
			//	g_object_set(G_OBJECT(videoencoder_), "qp-min", 15, NULL);

			guint curbps = 0;
			g_object_get(G_OBJECT(videoencoder_), "bps", &curbps, NULL);
			guint mxbpx = 0;
			g_object_get(G_OBJECT(videoencoder_), "bps-max", &mxbpx, NULL);

			//	printf("==================mpp 265 read============= bps=%d  max=%d\n", curbps, mxbpx);
		}
		else if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV) {
			videoencoder_ = gst_element_factory_make("nvv4l2h265enc", ("encoder" + std::to_string(tid_)).c_str());
			if (!videoencoder_) {
				puts("nvv encoder creating failed");
				return -1;
			}
		}
		else if (TaskManager::GetInstance()->chip_type_ == CHIP_SOFTWARE || TaskManager::GetInstance()->chip_type_ == CHIP_TRYCPU || pFlow_->enc_sel_ == 0) {
			videoencoder_ = gst_element_factory_make("x265enc", ("encoder110" + std::to_string(tid_)).c_str());
			if (videoencoder_ == NULL) {
				videoencoder_ = gst_element_factory_make("openh265enc", ("encoder12" + std::to_string(tid_)).c_str());
			}
			//default_fps_n_254_ = 25;
			puts("---------------- x265enc encoder create ----------------------");
			g_object_set(G_OBJECT(videoencoder_), "bitrate", pFlow_->bitrate_, NULL);
			//g_object_set(G_OBJECT(videoencoder_), "pass", pFlow_->rcmode_ == 0 ? 17 : 0, NULL);
		}


		TSJson::Value::Members mem = pFlow_->x265_extra_.getMemberNames();
		for (auto iter = mem.begin(); iter != mem.end(); iter++)
		{
			const char* paramname = (*iter).c_str();
			//	printf("set %s:", (char*)paramname);
			if (pFlow_->x265_extra_[*iter].type() == TSJson::stringValue)
			{
				const char* strVal = pFlow_->x265_extra_[*iter].asString().c_str();
				//	printf("%s \n", (char*)strVal);
				g_object_set(G_OBJECT(videoencoder_), paramname, strVal, NULL);

			}
			else if (pFlow_->x265_extra_[*iter].type() == TSJson::realValue)
			{
				float fVal = pFlow_->x265_extra_[*iter].asDouble();
				//	printf("%f \n", fVal);
				g_object_set(G_OBJECT(videoencoder_), paramname, fVal, NULL);
			}
			else if (pFlow_->x265_extra_[*iter].type() == TSJson::uintValue)
			{
				int iVal = pFlow_->x265_extra_[*iter].asUInt();
				//	printf("%d \n", pFlow_->x265_extra_[*iter].asUInt());
				g_object_set(G_OBJECT(videoencoder_), paramname, iVal, NULL);
			}
			else if (pFlow_->x265_extra_[*iter].type() == TSJson::intValue)
			{
				int iVal = pFlow_->x265_extra_[*iter].asInt();
				//		printf("%d \n", pFlow_->x265_extra_[*iter].asInt());
				g_object_set(G_OBJECT(videoencoder_), paramname, iVal, NULL);
			}
			else if (pFlow_->x265_extra_[*iter].type() == TSJson::booleanValue)
			{

				int iVal = pFlow_->x265_extra_[*iter].asBool();
				//		printf("%d \n", pFlow_->x265_extra_[*iter].asUInt());
				g_object_set(G_OBJECT(videoencoder_), paramname, iVal, NULL);
			}

		}
	}

	if (!videoencoder_) {
		g_printerr("video encoding create error.............. \n");
		return -1;

	}
	return 0;
}


void DirectTranscodingTask::on_sdp_callback(GstElement* rtspsrc, GstSDPMessage* sdp, gpointer udata)
{
	//gst_sdp_message_dump(sdp);
	DirectTranscodingTask* pTask = (DirectTranscodingTask*)udata;

	//int audiopt, videopt;
	int count = gst_sdp_message_medias_len(sdp);
	for (int i = 0; i < count; i++) {
		const GstSDPMedia* tmp = gst_sdp_message_get_media(sdp, i);
		printf("-----sdp media type =%s:%s\n", gst_sdp_media_get_media(tmp), gst_sdp_media_get_attribute_val(tmp, "rtpmap"));


		const char* checkstr = gst_sdp_media_get_attribute_val(tmp, "rtpmap");
		if (checkstr != NULL) {
			string mapstr = gst_sdp_media_get_attribute_val(tmp, "rtpmap");

			if (strcmp(gst_sdp_media_get_media(tmp), "audio") == 0) {
				pTask->pFlow_->haveaudio_ = 1;
				if (mapstr.find("PCMA") != string::npos) {

					pTask->audio_pt_ = 8;

				}
				if (mapstr.find("PCMU") != string::npos) {
					pTask->audio_pt_ = 0;

				}
				if (mapstr.find("G726") != string::npos) {
					pTask->audio_pt_ = 21;
				}

				if (mapstr.find("opus") != string::npos) {
					pTask->audio_pt_ = 101;

				}

			}

			if (strcmp(gst_sdp_media_get_media(tmp), "video") == 0) {
				pTask->pFlow_->haveaudio_ = 1;
				if (mapstr.find("H264") != string::npos) {
					pTask->video_pt_ = 96;
				}
				if (mapstr.find("H265") != string::npos) {
					pTask->video_pt_ = 98;

				}
				if (mapstr.find("JPEG") != string::npos) {
					pTask->video_pt_ = 26;

				}
				
			}
		}
		else {
			string mapstr = gst_sdp_media_get_attribute_val(tmp, "fmtp");

			if (strcmp(gst_sdp_media_get_media(tmp), "audio") == 0) {
				pTask->pFlow_->haveaudio_ = 1;
				if (mapstr.find("8") != string::npos) {

					pTask->audio_pt_ = 8;

				}
				if (mapstr.find("0") != string::npos) {
					pTask->audio_pt_ = 0;

				}
				if (mapstr.find("G726") != string::npos) {
					pTask->audio_pt_ = 21;
				}

				if (mapstr.find("opus") != string::npos) {
					pTask->audio_pt_ = 101;

				}

			}
		}
	}

	pTask->addPipelineDecoder();
	string outtype = pTask->pFlow_->outputtype_;
	if (outtype  == "rtsp") {
		pTask->addPipelineRtsp();
	}
	else if (outtype == "mp4" || outtype == "ts" || outtype == "flv") {
		pTask->addPipelineFile();
	}

	//pTask->completeRtspPipeline();


}


// destroy
void DirectTranscodingTask::releasePileline()
{

	if (loop_ != NULL) {
		g_main_loop_quit(loop_);
	}

	if (loop_ != NULL) {
		g_main_loop_unref(loop_);
		loop_ = NULL;
	}
	if (sharebuf_ != NULL)
	{
		free(sharebuf_);
		sharebuf_ = NULL;
	}

	isRun = STATUS_DISCONNECT;
}


void DirectTranscodingTask::setExtraParameter(GstElement* pElement, TSJson::Value& JsonObj) {
	TSJson::Value::Members mem = JsonObj.getMemberNames();
	for (auto iter = mem.begin(); iter != mem.end(); iter++)
	{
		const char* paramname = (*iter).c_str();
		DebugLog::writeLogF("set %s: type:%d val:", (char*)paramname, JsonObj[*iter].type());
		if (JsonObj[*iter].type() == TSJson::stringValue)
		{
			const char* strVal = JsonObj[*iter].asString().c_str();
			DebugLog::writeLogF("%s \n", (char*)strVal);
			g_object_set(G_OBJECT(pElement), paramname, strVal, NULL);

		}
		else if (JsonObj[*iter].type() == TSJson::realValue)
		{
			float fVal = JsonObj[*iter].asDouble();
			DebugLog::writeLogF("%f \n", fVal);
			g_object_set(G_OBJECT(pElement), paramname, fVal, NULL);
		}
		else if (JsonObj[*iter].type() == TSJson::uintValue)
		{
			int iVal = JsonObj[*iter].asUInt();
			DebugLog::writeLogF(" val %d \n", JsonObj[*iter].asUInt());
			g_object_set(G_OBJECT(pElement), paramname, iVal, NULL);
		}
		else if (JsonObj[*iter].type() == TSJson::intValue)
		{
			int iVal = JsonObj[*iter].asInt();
			DebugLog::writeLogF(" val %d \n", JsonObj[*iter].asInt());
			g_object_set(G_OBJECT(pElement), paramname, iVal, NULL);
		}
		else if (JsonObj[*iter].type() == TSJson::booleanValue)
		{

			int iVal = JsonObj[*iter].asBool();
			DebugLog::writeLogF("%d \n", JsonObj[*iter].asUInt());
			g_object_set(G_OBJECT(pElement), paramname, iVal, NULL);
		}

	}
}

GstPadProbeReturn DirectTranscodingTask::pad_probefps(GstPad* pad, GstPadProbeInfo* info, gpointer user_data)
{
	RtspTask* pTask = (RtspTask*)user_data;
	GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
	GstCaps* caps;
		//printf("=====================pad_probe fps=======================%d\n", GST_EVENT_TYPE(event));
	if (time(NULL) - pTask->pFlow_->task_start_time_ > 10) {
		pTask->myfps(0);

	}
	return GST_PAD_PROBE_OK;

}


GstPadProbeReturn DirectTranscodingTask::pad_probe2(GstPad* pad, GstPadProbeInfo* info, gpointer user_data)
{
	RtspTask* pTask = (RtspTask*)user_data;
	GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
	GstCaps* caps;
	//	printf("=====================pad_probe 2=======================%d\n", GST_EVENT_TYPE(event));


	(void)pad;

	if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
		return GST_PAD_PROBE_OK;

	//printf("=====================pad_probe 3=======================%d\n", GST_EVENT_TYPE(event));
	gst_event_parse_caps(event, &caps);
	if (!caps) {
		GST_ERROR("caps event without caps");
		return GST_PAD_PROBE_OK;
	}
	//printf("=====================pad_probe 4=======================%d\n", GST_EVENT_TYPE(event));

	GstVideoInfo infovideo;
	if (!gst_video_info_from_caps(&infovideo, caps)) {
		GST_ERROR("caps event with invalid video caps");
		return GST_PAD_PROBE_OK;
	}
	if (pTask->m_source_width != 0 && pTask->m_source_height != 0) {

		if (pTask->m_source_width != GST_VIDEO_INFO_WIDTH(&infovideo) || pTask->m_source_height != GST_VIDEO_INFO_HEIGHT(&infovideo)) {
			pTask->stop(7);
		}
	}
	printf("=====================pad_probe 5=======================%d  wh=%d  %d\n", GST_EVENT_TYPE(event), GST_VIDEO_INFO_WIDTH(&infovideo), GST_VIDEO_INFO_HEIGHT(&infovideo));

	return GST_PAD_PROBE_OK;

}


GstPadProbeReturn DirectTranscodingTask::pad_probe(GstPad* pad, GstPadProbeInfo* info, gpointer user_data)
{
	DirectTranscodingTask* pTask = (DirectTranscodingTask*)user_data;
	GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
	GstCaps* caps;

	(void)pad;
	printf("=====================pad_probe 2=======================%d\n", GST_EVENT_TYPE(event));
	//puts(">>>>>>>>>>>>>>.  DirectTranscodingTask::pad_probe\n\n\n");

	//return GST_PAD_PROBE_OK;
	if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
		return GST_PAD_PROBE_OK;

	gst_event_parse_caps(event, &caps);

	if (!caps) {
		GST_ERROR("caps event without caps");
		return GST_PAD_PROBE_OK;
	}

	if (!gst_video_info_from_caps(&pTask->videoinfo_, caps)) {
		GST_ERROR("caps event with invalid video caps");
		return GST_PAD_PROBE_OK;
	}
	printf("=======   video size  = %d   %d\n", GST_VIDEO_INFO_WIDTH(&(pTask->videoinfo_)), GST_VIDEO_INFO_HEIGHT(&(pTask->videoinfo_)));
	DebugLog::writeLogF("++++++++++++++ video info: offset: %d  %d; stride:%d %d ;size: %d ; fps: %d %d; alpha:%d ;interlace:%d\n",
		GST_VIDEO_INFO_PLANE_OFFSET(&(pTask->videoinfo_), 0),
		GST_VIDEO_INFO_PLANE_OFFSET(&(pTask->videoinfo_), 1),
		GST_VIDEO_INFO_PLANE_STRIDE(&(pTask->videoinfo_), 0),
		GST_VIDEO_INFO_PLANE_STRIDE(&(pTask->videoinfo_), 1),
		GST_VIDEO_INFO_SIZE(&(pTask->videoinfo_)),
		GST_VIDEO_INFO_FPS_N(&(pTask->videoinfo_)),
		GST_VIDEO_INFO_FPS_D(&(pTask->videoinfo_)),
		GST_VIDEO_INFO_HAS_ALPHA(&(pTask->videoinfo_)),
		GST_VIDEO_INFO_IS_INTERLACED(&(pTask->videoinfo_)));
	switch (GST_VIDEO_INFO_FORMAT(&(pTask->videoinfo_))) {
	case GST_VIDEO_FORMAT_I420:
		pTask->format_ = 2;
		break;
	case GST_VIDEO_FORMAT_NV12:
		pTask->format_ = 23;
		break;
	case GST_VIDEO_FORMAT_YUY2:
		pTask->format_ = 4;
		break;
	default:
		GST_ERROR("unknown format\n");
		return GST_PAD_PROBE_OK;
	}

	pTask->isRun = STATUS_CONNECTED;

	return GST_PAD_PROBE_OK;
}
GstPadProbeReturn DirectTranscodingTask::pad_probe1(GstPad* pad, GstPadProbeInfo* info, gpointer user_data)
{
	DirectTranscodingTask* pTask = (DirectTranscodingTask*)user_data;
	GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
	GstCaps* caps;

	(void)pad;

	if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
		return GST_PAD_PROBE_OK;

	gst_event_parse_caps(event, &caps);

	if (!caps) {
		GST_ERROR("caps event without caps");
		return GST_PAD_PROBE_OK;
	}

	if (!gst_audio_info_from_caps(&pTask->audioinfo_, caps)) {
		GST_ERROR("caps event with invalid video caps");
		return GST_PAD_PROBE_OK;
	}

	printf("audio ---- >>>>>>>>>>>>>>>>>>>>>>>>>>>>> ch=%d fomt=%d sample rate=%d\n", GST_AUDIO_INFO_CHANNELS(&pTask->audioinfo_), GST_AUDIO_INFO_FORMAT(&pTask->audioinfo_), GST_AUDIO_INFO_RATE(&pTask->audioinfo_));

	/*switch (GST_VIDEO_INFO_FORMAT(&(pTask->videoinfo_))) {
	case GST_VIDEO_FORMAT_I420:
		pTask->format_ = 2;
		break;
	case GST_VIDEO_FORMAT_NV12:
		pTask->format_ = 23;
		break;
	case GST_VIDEO_FORMAT_YUY2:
		pTask->format_ = 4;
		break;
	default:
		GST_ERROR("unknown format\n");
		return GST_PAD_PROBE_OK;
	}*/

	pTask->isRun = STATUS_CONNECTED;

	return GST_PAD_PROBE_OK;
}

// rtsp init

int DirectTranscodingTask::buildRtsp2RtspPipeline() {

	// Build Pipeline 
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	streamsrc_ = gst_element_factory_make("rtspsrc", ("filesrc0" + std::to_string(tid_)).c_str());
	demuxer_ = gst_element_factory_make("rtph264depay", ("videodemux" + std::to_string(tid_)).c_str());
	videoparser_ = gst_element_factory_make("h264parse", ("parser" + std::to_string(tid_)).c_str());
	videodecoder_ = gst_element_factory_make("mppvideodec", ("videodecode" + std::to_string(tid_)).c_str());

	//vconverter_ = gst_element_factory_make("videoconvert", ("videoconv" + std::to_string(tid_)).c_str());
	//videoencoder_ = gst_element_factory_make("mpph264enc", ("videoenc" + std::to_string(tid_)).c_str());
	selectVideoEncoder();

	videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	rtsppushsink_ = gst_element_factory_make("rtspclientsink", ("videoapp_sink" + std::to_string(tid_)).c_str());



	if (!pipeline_ | !videodecoder_ || !videoqueue_ || !rtsppushsink_) {
		g_printerr("One element could not be created 2.\n");
		return -1;
	}

	// appsink setting
	g_object_set(G_OBJECT(rtsppushsink_), "location", pFlow_->outputpath_.c_str(), NULL);
	g_object_set(G_OBJECT(streamsrc_), "location", pFlow_->rtspUrl_.c_str(), NULL);
	g_object_set(G_OBJECT(streamsrc_), "latency", pFlow_->delay_input_, NULL);
	g_object_set(GST_OBJECT(streamsrc_), "debug", true, NULL);
	g_signal_connect(streamsrc_, "on-sdp", G_CALLBACK(DirectTranscodingTask::on_sdp_callback), this);
	g_signal_connect(streamsrc_, "before-send", G_CALLBACK(before_send_callback), this);


	//g_object_set(GST_OBJECT(streamsrc_), "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
	//Partially static pipeline link
	gst_bin_add_many(GST_BIN(pipeline_), streamsrc_, demuxer_, videoparser_, videodecoder_, vconverter_, videoencoder_, videoqueue_, rtsppushsink_, NULL);

	if (!gst_element_link_many(demuxer_, videoparser_, videodecoder_, vconverter_, videoencoder_, videoqueue_, rtsppushsink_, NULL)) {
		g_printerr("appsink Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	gst_pad_add_probe(gst_element_get_static_pad(streamsrc_, "src"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe, this, NULL);


	g_signal_connect(streamsrc_, "pad-added", G_CALLBACK(on_rtsp2dec_link), this);


	return 0;
}

int DirectTranscodingTask::buildRtsp2RtspPipeline2() {

	// Build Pipeline 
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	streamsrc_ = gst_element_factory_make("rtspsrc", ("filesrc0" + std::to_string(tid_)).c_str());
	//demuxer_ = gst_element_factory_make("rtph264depay", ("filedemux" + std::to_string(tid_)).c_str());
	//videoparser_ = gst_element_factory_make("h264parse", ("parser" + std::to_string(tid_)).c_str());
	videodecoder_ = gst_element_factory_make("mppvideodec", ("videodecode" + std::to_string(tid_)).c_str());

	//vconverter_ = gst_element_factory_make("videoconvert", ("videoconv" + std::to_string(tid_)).c_str());
	//videoencoder_ = gst_element_factory_make("mpph264enc", ("videoenc" + std::to_string(tid_)).c_str());
	selectVideoEncoder();

	videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	rtsppushsink_ = gst_element_factory_make("rtspclientsink", ("videoapp_sink" + std::to_string(tid_)).c_str());





	if (!pipeline_ | !videodecoder_ || !videoqueue_ || !rtsppushsink_) {
		g_printerr("One element could not be created 2.\n");
		return -1;
	}

	// appsink setting
	g_object_set(G_OBJECT(rtsppushsink_), "location", pFlow_->outputpath_.c_str(), NULL);
	g_object_set(G_OBJECT(streamsrc_), "location", pFlow_->rtspUrl_.c_str(), NULL);
	g_object_set(G_OBJECT(streamsrc_), "latency", pFlow_->delay_input_, NULL);
	g_object_set(GST_OBJECT(streamsrc_), "debug", true, NULL);
	g_signal_connect(streamsrc_, "on-sdp", G_CALLBACK(DirectTranscodingTask::on_sdp_callback), this);
	g_signal_connect(streamsrc_, "before-send", G_CALLBACK(before_send_callback), this);


	//g_object_set(GST_OBJECT(streamsrc_), "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
	//Partially static pipeline link
	gst_bin_add_many(GST_BIN(pipeline_), streamsrc_, videodecoder_, vconverter_, videoencoder_, videoqueue_, rtsppushsink_, NULL);

	if (!gst_element_link_many(videodecoder_, vconverter_, videoencoder_, videoqueue_, rtsppushsink_, NULL)) {
		g_printerr("appsink Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	//addAudioBranch(8);

	//addVideoBranch(98);


	g_signal_connect(streamsrc_, "pad-added", G_CALLBACK(on_rtsp2dec_link), this);


	return 0;
}

