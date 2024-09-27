#include "RtpTask.h"
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

RtpTask::RtpTask(TaskFlow* pFlow)
{
	pFlow_ = pFlow;
	//strUrl_ = pFlow->rtpUrl_; //
	withaudio_ = pFlow->withaudio_;
	tid_ = TaskManager::total_tid++;
	gst_init(NULL, NULL);
	puts("create RtpTask");
}


RtpTask::~RtpTask() {
	puts("~delete RtpTask");
	return;
	//releasePileline();
	reportStatus();
	Packet* newData = new Packet();
	pFlow_->pRawData_in->push(newData);
	Packet* newData1 = new Packet();
	pFlow_->pAudioData_in->push(newData1);
	pFlow_->onComplete(2);
}

int RtpTask::parseSDP() {
	/*
	string testsdp = "v = 0\r\n";
	testsdp += "o=31010450151188027324 0 0 IN IP4 192.168.110.249\r\n";
	testsdp += "s=Play\r\n";
	testsdp += "c=IN IP4 192.168.110.249\r\n";
	testsdp += "t=0 0\r\n";
	testsdp += "m=video 62000 TCP/RTP/AVP 96\r\n";
	testsdp += "a=sendonly\r\n";
	testsdp += "a=rtpmap:96 PS/90000\r\n";
	testsdp += "a=setup:active\r\n";
	testsdp += "y=0000127326\r\n";

	puts(testsdp.c_str());
	*/
	setup_type_ = 0;//1, active 0,passive
	passive_trans_ = 0; //0 udp, 1 tcp
	listen_port = 8000;
	peer_port = 10004;
	peer_host = "0.0.0.0";
	video_codec_ = "H264";

	if (pFlow_->in_rtp_["setup"].isString())
	{
		string tmp = pFlow_->in_rtp_["setup"].asString();
		setup_type_ = tmp == "active" ? 1 : 0;
	}
	if (pFlow_->in_rtp_["transport"].isString())
	{
		string tmp = pFlow_->in_rtp_["transport"].asString();
		passive_trans_ = tmp == "tcp" ? 1 : 0;
	}
	
	if (pFlow_->in_rtp_["host"].isString())
	{
		string tmp = pFlow_->in_rtp_["host"].asString();
		peer_host = tmp;
	}
	if (pFlow_->in_rtp_["port"].isInt())
	{
		peer_port = pFlow_->in_rtp_["port"].asInt();
	}
	if (pFlow_->in_rtp_["video_pt"].isInt())
	{
		video_pt = pFlow_->in_rtp_["video_pt"].asInt();
	}
	if (pFlow_->in_rtp_["audio_pt"].isInt())
	{
		peer_port = pFlow_->in_rtp_["audio_pt"].asInt();
	}
	if (pFlow_->in_rtp_["ssrc"].isString())
	{
		ssrc_ = pFlow_->in_rtp_["ssrc"].asString();
	}

	if (setup_type_ == 0) {
		listen_port = peer_port;
	}

	printf("---rtp src info:addr %s %d,vpt=%d trans=%d\n", peer_host.c_str(), listen_port, video_pt, passive_trans_);

	return 0;
	
/*
	GstSDPMessage * sdpmsg;
	int ret = gst_sdp_message_new(&sdpmsg);
	int ret1 =gst_sdp_message_new_from_text(testsdp.c_str(), &sdpmsg);

	//gst_sdp_message_dump(sdpmsg);

	puts(gst_sdp_message_as_text(sdpmsg));

	

	
	
	int count = gst_sdp_message_medias_len(sdpmsg);
	printf("--------ret = %d %d %d\n\n", ret, ret1, count);
	for (int i = 0; i < count; i++) {
		const GstSDPMedia * tmp = gst_sdp_message_get_media(sdpmsg, i);
		printf("-----sdp media type = %s\n", gst_sdp_media_get_media(tmp));
		
	}

	count = gst_sdp_message_attributes_len(sdpmsg);
	printf("attr count=%d\n",count);
	for (int i = 0; i < count; i++) {
		const GstSDPAttribute * tmp = gst_sdp_message_get_attribute(sdpmsg, i);
		//printf("-----sdp attr type = %s\n", tmp);

	}
	*/
	
	return 0;
}
void RtpTask::reportStatus()
{
	/*
		rtp task closed
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
	else if (closestyle_ == 2) {
		root["code"] = ERR_RTSP_LOST;
		root["message"] = closeMessage_;
	}
	else if (closestyle_ == 5) {
		root["code"] = ERR_DECODE_ERR;
		root["message"] = "build pipeline fail, check gstreamer plugin on the device";

	}
	else if (closestyle_ == 6) {
		root["code"] = ERR_SRC_AV_OUT_SYNC;
		root["message"] = "src rtp audio and video is out sync, you need restart this task";

	}

	TaskManager::GetInstance()->onAnyWorkStatus(root, 0);

}
void *RtpTask::RtpTask::startThread(TaskFlow* pFlow)
{
	RtpTask* thread = new RtpTask(pFlow);

	thread->setAutoDelete(true);
	thread->start();

	return thread;
}
void RtpTask::stop()
{

	closestyle_ = 1;
	needteardown_ = 1;
//	bool ret = gst_bus_post(bus_, gst_message_new_eos(GST_OBJECT_CAST(trans_layer_src_)));
	//printf("stop rtp task--------------------%d %s \n", ret, pFlow_->taskid_.c_str());

}
bool RtpTask::RtpTask::onStart()
{
	if (0 != parseSDP())
		return false;
	return true;
}
void RtpTask::RtpTask::run()
{

	rtsp_rtp_transmode_ = TCP_CONN_MODE;
	isRun = STATUS_CONNECTING;

	DebugLog::writeLogF("start connect rtp url: %s \n", pFlow_->rtspUrl_.c_str());


	int ret = 0;
	ret = buildPipeline();
	if (ret < 0)
	{
		isRun = STATUS_DISCONNECT;
		DebugLog::writeLogF("rtp build pipe line failed :%s\n", pFlow_->rtspUrl_.c_str());
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

		do {
		//	printf("---- start handle gst message-----\n\n");
			GstMessage* msg = gst_bus_timed_pop_filtered(bus_, 200 * GST_MSECOND, (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

			if (msg != NULL) {
				GError *err;
				gchar *debug_info;

				switch (GST_MESSAGE_TYPE(msg)) {
				case GST_MESSAGE_ERROR:
					gst_message_parse_error(msg, &err, &debug_info);
					g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
					closeMessage_ = err->message;
					closestyle_ = 2;
					g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
					g_clear_error(&err);
					g_free(debug_info);
					terminate = TRUE;
					break;
				case GST_MESSAGE_EOS:
					g_print("End-Of-Stream in rtp.\n");
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
					//printf("redistributing latency\n");
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


int RtpTask::buildPipeline() {

	// Build Pipeline 
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}

	if (setup_type_ == 0) {
		if (passive_trans_ == 0) {
			trans_layer_src_ = gst_element_factory_make("udpsrc", ("udpsrc" + std::to_string(tid_)).c_str());
		}
		else {
			trans_layer_src_ = gst_element_factory_make("tcpserversrc", ("tcpserversrc" + std::to_string(tid_)).c_str());
			g_object_set(GST_OBJECT(trans_layer_src_), "host","0.0.0.0", NULL);

		}

		g_object_set(GST_OBJECT(trans_layer_src_), "port", listen_port, NULL);
	}
	else {
		trans_layer_src_ = gst_element_factory_make("tcpclientsrc", ("tcpclientsrc" + std::to_string(tid_)).c_str());

		g_object_set(GST_OBJECT(trans_layer_src_), "port", peer_port, NULL);
		g_object_set(GST_OBJECT(trans_layer_src_), "host", peer_host.c_str(), NULL);

	}
	
	rtpbin_ = gst_element_factory_make("rtpbin", ("rtpbin" + std::to_string(tid_)).c_str());


	appvideosink_ = gst_element_factory_make("appsink", ("appsink" + std::to_string(tid_)).c_str());
	


	if (!pipeline_ || !trans_layer_src_ | !rtpbin_ ||!appvideosink_) {
		g_printerr("One element could not be created 2.\n");
		return -1;
	}

	// appsink setting

	g_object_set(G_OBJECT(appvideosink_), "sync", FALSE, NULL);
	GstPad * apppad = gst_element_get_static_pad(appvideosink_, "sink");
	gst_pad_add_probe(apppad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, appsink_query_cb, NULL, NULL);
	gst_object_unref(apppad);
	gst_base_sink_set_max_lateness(GST_BASE_SINK(appvideosink_), 70 * GST_MSECOND);
	gst_base_sink_set_qos_enabled(GST_BASE_SINK(appvideosink_), TRUE);
	g_object_set(G_OBJECT(appvideosink_), "max-buffers", this->pFlow_->buf_input_ / 3, NULL);
	gst_pad_add_probe(gst_element_get_static_pad(appvideosink_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe, this, NULL);
	g_object_set(G_OBJECT(appvideosink_), "emit-signals", TRUE, NULL);
	g_signal_connect(G_OBJECT(appvideosink_), "new-sample", G_CALLBACK(RtpTask::on_video_sample), this);
	
	//rtp setting;

	GstCaps *caps = gst_caps_new_simple("application/x-rtp", \
		"media", G_TYPE_STRING, "video", \
		"encoding-name", G_TYPE_STRING, video_codec_.c_str(), \
		"clock-rate", G_TYPE_INT, 90000, \
		NULL);
	g_object_set(GST_OBJECT(trans_layer_src_), "caps", caps, NULL);
	gst_caps_unref(caps);


	GstPadTemplate *mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(rtpbin_), "recv_rtp_sink_%u");
	if (mux_src_pad_template == NULL)		g_printerr("recv_rtp_sink_pad_template null. \n");

	GstPad * muxer_pad = gst_element_request_pad(rtpbin_, mux_src_pad_template, NULL, NULL);
	if (muxer_pad == NULL)		g_printerr("muxer_pad null. \n");
	//	gst_object_unref(muxer_pad);



	GstPadTemplate *mux_src_pad_template1 = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(rtpbin_), "recv_rtp_src_%u");
	if (mux_src_pad_template1 == NULL)		g_printerr("recv_rtp_sink_pad_template null. \n");

	GstPad * muxer_pad1 = gst_element_request_pad(rtpbin_, mux_src_pad_template1, NULL, NULL);
	if (muxer_pad1 == NULL)		g_printerr("muxer_pad null. \n");
	


	//Partially static pipeline link
	gst_bin_add_many(GST_BIN(pipeline_), trans_layer_src_, rtpbin_,  appvideosink_, NULL);

	addVideocodec(98);

	/*if (addAudioBranch() < 0) {
		return -1;
	}*/


	if (!gst_element_link_many(trans_layer_src_, rtpbin_, NULL)) {
		g_printerr("rtp Elements could not be linked 1.\n");
		gst_object_unref(pipeline_);
		return -1;
	}



	gst_object_unref(muxer_pad);
	gst_object_unref(muxer_pad1);

	g_signal_connect(rtpbin_, "pad-added", G_CALLBACK(on_rtpbin2dec_link), this);
	g_signal_connect(rtpbin_, "request-pt-map", G_CALLBACK(onClockrateRequest), this);

	return 0;
}



int RtpTask::addAudioBranch()
{
	if (withaudio_ > 0) {
		aconverter_ = gst_element_factory_make("audioconvert", ("aconv" + std::to_string(tid_)).c_str());
		audioqueue_ = gst_element_factory_make("queue", ("queue_fakesink" + std::to_string(tid_)).c_str());
		appaudiosink_ = gst_element_factory_make("appsink", ("appaudiosink_" + std::to_string(tid_)).c_str());
		audiodecoder_ = gst_element_factory_make("decodebin", ("adecodebin" + std::to_string(tid_)).c_str());
		if (!aconverter_ || !audioqueue_ || !audiodecoder_ || !appaudiosink_) {
			g_printerr("rtp audio element could not be created.\n");
			return -1;
		}
#if defined(NO_ENHANCE)
		audiocpasfilter_ = gst_element_factory_make("capsfilter", NULL);
		if (!audiocpasfilter_) {
			puts("---audio caps filter created failed ---");
		}

		{
			puts("+++++++++++++++++++================================++++++++++++++++++++");
			gst_util_set_object_arg(G_OBJECT(audiocpasfilter_), "caps",
				"audio/x-raw, "
				"format={ S16LE},rate=8000,channels=1");
		}
		audioresamp_ = gst_element_factory_make("audioresample", NULL);
		audiorate_ = gst_element_factory_make("audiorate", ("audiorate" + std::to_string(tid_)).c_str());
#else
		audiocpasfilter_ = gst_element_factory_make("capsfilter", NULL);
		if (!audiocpasfilter_) {
			puts("---audio caps filter created failed ---");
		}

		if (pFlow_->withfilter_ == 1)
		{
			gst_util_set_object_arg(G_OBJECT(audiocpasfilter_), "caps",
				"audio/x-raw, "
				"format={ F32LE},rate=16000");
		}
		else
		{
			puts("+++++++++++++++++++================================++++++++++++++++++++");
			gst_util_set_object_arg(G_OBJECT(audiocpasfilter_), "caps",
				"audio/x-raw, "
				"format={ S16LE},rate=8000");
		}
		audioresamp_ = gst_element_factory_make("audioresample", NULL);

#endif
		g_object_set(G_OBJECT(appaudiosink_), "sync", FALSE, NULL);
		gst_base_sink_set_max_lateness(GST_BASE_SINK(appaudiosink_), 70 * GST_MSECOND);
		gst_base_sink_set_qos_enabled(GST_BASE_SINK(appaudiosink_), TRUE);
		g_object_set(G_OBJECT(appaudiosink_), "max-buffers", this->pFlow_->buf_input_ , NULL);
		g_object_set(G_OBJECT(appaudiosink_), "emit-signals", TRUE, NULL);
		gst_pad_add_probe(gst_element_get_static_pad(appaudiosink_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe1, this, NULL);
		g_signal_connect(G_OBJECT(appaudiosink_), "new-sample", G_CALLBACK(RtpTask::on_audio_sample), this);
#if defined(NO_ENHANCE)
		//gst_bin_add_many(GST_BIN(pipeline_), audiodecoder_, aconverter_, audioqueue_, appaudiosink_, NULL);
		gst_bin_add_many(GST_BIN(pipeline_), audiodecoder_, aconverter_, audiorate_, audioresamp_, audiocpasfilter_, audioqueue_, appaudiosink_, NULL);
#else
		gst_bin_add_many(GST_BIN(pipeline_), audiodecoder_, aconverter_, audioresamp_, audiocpasfilter_, audioqueue_, appaudiosink_, NULL);
#endif	

		if (!gst_element_sync_state_with_parent(aconverter_)) {
			g_printerr("sync audio element stat faile 0.\n");

		}

		if (!gst_element_sync_state_with_parent(audioqueue_))
		{
			g_printerr("sync audio element stat faile 1.\n");

		}
		if (!gst_element_sync_state_with_parent(appaudiosink_)) {
			g_printerr("sync audio element stat faile 2.\n");

		}
		if (!gst_element_sync_state_with_parent(audiodecoder_)) {
			g_printerr("sync audio element stat faile 3.\n");

		}
		//#if defined(NO_ENHANCE)
		//#else
		if (!gst_element_sync_state_with_parent(audiocpasfilter_)) {
			g_printerr("sync audio element stat faile 4.\n");

		}
		if (!gst_element_sync_state_with_parent(audioresamp_)) {
			g_printerr("sync audio element stat faile 5.\n");

		}
		if (!gst_element_sync_state_with_parent(audiorate_)) {
			g_printerr("sync audio element stat faile 5.\n");

		}
		//#endif

		g_signal_connect(audiodecoder_, "pad-added", G_CALLBACK(on_dec2conv_link), this);

		shareAudiobuf_ = (char *)malloc(2048);

	}

	return 0;

}

// destroy
void RtpTask::releasePileline()
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
	if (shareAudiobuf_ != NULL)
	{
		free(shareAudiobuf_);
		shareAudiobuf_ = NULL;
	}

	isRun = STATUS_DISCONNECT;
}


GstPadProbeReturn RtpTask::pad_probe(GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
	RtpTask *pTask = (RtpTask *)user_data;
	GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
	GstCaps *caps;

	(void)pad;

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
	puts("---- >>>>>>>>>>>>>>>> vidoe info\n");
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
GstPadProbeReturn RtpTask::pad_probe1(GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
	RtpTask *pTask = (RtpTask *)user_data;
	GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
	GstCaps *caps;

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
	pTask->pFlow_->audiochannel = GST_AUDIO_INFO_CHANNELS(&pTask->audioinfo_);
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

GstCaps * RtpTask::onClockrateRequest(GstElement * buffer, guint sessionid, guint pt, gpointer udata) {
	printf("++++++++++++++++++++++++   clock rate++++++ %d %d\n\n\n", sessionid,pt);
	
	if (pt == 0) {
		GstCaps *caps = gst_caps_new_simple("application/x-rtp", \
			"media", G_TYPE_STRING, "audio", \
			"encoding-name", G_TYPE_STRING, "PCMA", \
			"clock-rate", G_TYPE_INT, 8000, \
			NULL);

		return caps;
	}
	else if (pt == 98) 
	{
		GstCaps *caps = gst_caps_new_simple("application/x-rtp", \
			"media", G_TYPE_STRING, "video", \
			"encoding-name", G_TYPE_STRING, "H264", \
			"clock-rate", G_TYPE_INT, 90000, \
			NULL);

		return caps;
	}

}

int RtpTask::addVideocodec(int pt)
{
	if (pt == 98) {
		
		if (video_codec_ == "H264") {
			videodepay_ = gst_element_factory_make("rtph264depay", ("rtph264depay" + std::to_string(tid_)).c_str());
			if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV && pFlow_->dec_sel_ != 0) {
				puts("--- we select hardware dec for h264");
				videodec_ = gst_element_factory_make("avdec_h264", ("nvh264dec" + std::to_string(tid_)).c_str());
			}
			else {
				puts("--- we select software dec for h264");
				videodec_ = gst_element_factory_make("avdec_h264", ("avdec_h264" + std::to_string(tid_)).c_str());
			}
		}
		else if (video_codec_ == "H265") {
			videodepay_ = gst_element_factory_make("rtph265depay", ("rtph265depay" + std::to_string(tid_)).c_str());
			if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV && pFlow_->dec_sel_ != 0) {
				videodec_ = gst_element_factory_make("nvh265dec", ("nvh265dec" + std::to_string(tid_)).c_str());
			}
			else {
				videodec_ = gst_element_factory_make("avdec_h265", ("avdec_h265" + std::to_string(tid_)).c_str());
			}
			
		}
	}
	else {
		puts("--- we need handle this payload later ---");
	}

	vconverter_ = gst_element_factory_make("videoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	cpasfilter_ = gst_element_factory_make("capsfilter", NULL);
	if (!cpasfilter_) {
		puts("--- caps filter created failed ---");
	}

	gst_util_set_object_arg(G_OBJECT(cpasfilter_), "caps",
		"video/x-raw, "
		"format={  NV12}");

	gst_bin_add_many(GST_BIN(pipeline_),   videodepay_, videodec_, vconverter_, cpasfilter_, NULL);

	if (!gst_element_link_many(videodepay_, videodec_, vconverter_, cpasfilter_, appvideosink_, NULL)) {
		g_printerr("rtp Elements could not be linked 2.\n");
		gst_object_unref(pipeline_);
		return -1;
	}
	puts("----------link depay and codec-----------");
	return 0;

}

void RtpTask::on_rtpbin2dec_link(GstElement *element, GstPad *pad, gpointer data) {
	// GstPad *sinkpad;
	RtpTask *pTask = (RtpTask *)data;
	g_print("rtp payload received new pad: '%s' from '%s':\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(element));

/*	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;
	new_pad_caps = gst_pad_get_current_caps(pad);
	new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
	new_pad_type = gst_structure_get_name(new_pad_struct);

	*/

	string tmpstr = GST_PAD_NAME(pad);
	int pos = tmpstr.rfind('_');
	tmpstr = tmpstr.substr(pos + 1);
	int payloadtype = atoi(tmpstr.c_str());

	printf("------------------++++++++++++++++++++ pad type: %s\n", tmpstr.c_str());

	if (payloadtype == 98 || payloadtype == 97 || payloadtype == 96) {
		//pTask->addVideocodec(98);

		GstPad * depay_sinkpad = gst_element_get_static_pad(pTask->videodepay_, "sink");
		if (gst_pad_is_linked(depay_sinkpad)) {
			g_print("already linked. Ignoring.\n");
			gst_object_unref(depay_sinkpad);

		}
	    {
			gst_pad_link(pad, depay_sinkpad);
			gst_object_unref(depay_sinkpad);
		}

		//gst_object_unref(depay_sinkpad);
	}
	else if (payloadtype == 0 || payloadtype == 8) {
		pTask->addAudioBranch();

		if (!gst_element_link_many(pTask->aconverter_, pTask->audioresamp_, pTask->audiocpasfilter_, pTask->audioqueue_, pTask->appaudiosink_, NULL)) {
			g_printerr("audio branch Elements could not be linked.\n");
			gst_object_unref(pTask->pipeline_);
			return;
		}
		GstPad * decode_sinkpad = gst_element_get_static_pad(pTask->audiodecoder_, "sink");
		if (gst_pad_is_linked(decode_sinkpad)) {
			g_print("We are already linked. Ignoring.\n");
			gst_object_unref(decode_sinkpad);
			return;

		}

		if (GST_PAD_LINK_OK != gst_pad_link(pad, decode_sinkpad))
		{
			g_print("pad link :rtsp---audiodecoder failed.\n");

		}
		gst_object_unref(decode_sinkpad);
	}
	
//	gst_caps_unref(new_pad_caps);

}



void RtpTask::on_dec2conv_link(GstElement *element, GstPad *pad, gpointer data) {
	// GstPad *sinkpad;
	RtpTask *pTask = (RtpTask *)data;
	g_print("video decoder received new pad: '%s' from '%s':\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(element));

	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;
	new_pad_caps = gst_pad_get_current_caps(pad);
	new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
	new_pad_type = gst_structure_get_name(new_pad_struct);
	printf("++++++++++++++++++++ pad type: %s\n", new_pad_type);
	if (g_str_has_prefix(new_pad_type, "audio/x-raw")) {
		GstPad * tee_sinkpad = gst_element_get_static_pad(pTask->aconverter_, "sink");
		if (gst_pad_is_linked(tee_sinkpad)) {
			g_print("already linked. Ignoring.\n");
			gst_object_unref(tee_sinkpad);

		}
		gst_pad_link(pad, tee_sinkpad);
		gst_object_unref(tee_sinkpad);
	}
	else
	{
		GstPad * tee_sinkpad = gst_element_get_static_pad(pTask->videodec_, "sink");
		if (gst_pad_is_linked(tee_sinkpad)) {
			g_print("already linked. Ignoring.\n");
			gst_object_unref(tee_sinkpad);
		}
		gst_pad_link(pad, tee_sinkpad);
		gst_object_unref(tee_sinkpad);
	}

	gst_caps_unref(new_pad_caps);

}


// appsink query
GstPadProbeReturn RtpTask::appsink_query_cb(GstPad * pad G_GNUC_UNUSED, GstPadProbeInfo * info, gpointer user_data G_GNUC_UNUSED)
{
	GstQuery *query = (GstQuery *)info->data;

	if (GST_QUERY_TYPE(query) != GST_QUERY_ALLOCATION)
		return GST_PAD_PROBE_OK;

	gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);

	return GST_PAD_PROBE_HANDLED;
}

// bus
gboolean RtpTask::onBusMessage(GstBus * bus, GstMessage * msg, gpointer user_data)
{
	RtpTask *pTask = (RtpTask *)user_data;


	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_UNKNOWN: {
		g_print("piple call_bus message GST_MESSAGE_UNKNOWN %d \n", pTask->tid_);
		break;
	}
	case GST_MESSAGE_STATE_CHANGED: {
		gchar *dotfilename;
		GstState old_gst_state, cur_gst_state, pending_gst_state;

		/* Only consider state change messages coming from
		 * the toplevel element. */
		if (GST_MESSAGE_SRC(msg) != GST_OBJECT(pTask->pipeline_))
			break;

		gst_message_parse_state_changed(msg, &old_gst_state, &cur_gst_state,
			&pending_gst_state);

		/*printf("GStreamer state change:  old: %s  current: %s  pending: %s\n",
			gst_element_state_get_name(old_gst_state),
			gst_element_state_get_name(cur_gst_state),
			gst_element_state_get_name(pending_gst_state)
		);*/

		dotfilename = g_strdup_printf("statechange__old-%s__cur-%s__pending-%s",
			gst_element_state_get_name(old_gst_state),
			gst_element_state_get_name(cur_gst_state),
			gst_element_state_get_name(pending_gst_state)
		);
		GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(pTask->pipeline_),
			GST_DEBUG_GRAPH_SHOW_ALL, dotfilename);

		// g_print ( "piple call_bus message GST_MESSAGE_STATE_CHANGED %d \n",dec->m_Id);

		g_free(dotfilename);

		break;
	}
	case GST_MESSAGE_REQUEST_STATE: {
		GstState requested_state;
		gst_message_parse_request_state(msg, &requested_state);
		DebugLog::writeLogF("state change to %s was requested by %s\n",
			gst_element_state_get_name(requested_state),
			GST_MESSAGE_SRC_NAME(msg)
		);
		gst_element_set_state(GST_ELEMENT(pTask->pipeline_), requested_state);
		break;
	}
	case GST_MESSAGE_LATENCY: {
		printf("redistributing latency\n");
		gst_bin_recalculate_latency(GST_BIN(pTask->pipeline_));
		break;
	}
	case GST_MESSAGE_EOS:
		g_print("bus eos \n");
		g_main_loop_quit(pTask->loop_);
		// dec->isRun = STATUS_DISCONNECT;
		return false;
		break;
	case GST_MESSAGE_INFO:
		break;
	case GST_MESSAGE_WARNING:
		break;
	case GST_MESSAGE_ERROR: {
		g_print("piple call_bus error message \n %s \n", pTask->pFlow_->rtspUrl_.c_str());
		GError *error = NULL;
		gchar *debug_info = NULL;
		gchar const *prefix = "";

		switch (GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_INFO:
			gst_message_parse_info(msg, &error, &debug_info);
			prefix = "INFO";
			break;
		case GST_MESSAGE_WARNING:
			gst_message_parse_warning(msg, &error, &debug_info);
			prefix = "WARNING";
			break;
		case GST_MESSAGE_ERROR:
			gst_message_parse_error(msg, &error, &debug_info);
			prefix = "ERROR";
			break;
		default:
			g_assert_not_reached();
		}
		g_print("GStreamer %s: %s; debug info: %s \n", prefix, error->message,
			debug_info);

		g_clear_error(&error);
		g_free(debug_info);

		if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
			GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(pTask->pipeline_),
				GST_DEBUG_GRAPH_SHOW_ALL, "error");
		}

		return false;

		break;
	}
	default:
		break;
	}

	return TRUE;
}//-----------------------------------------above static-----------------------------------------
// rtp init

int cpNV12_2(unsigned char* dst, unsigned char* src, int width, int height, int padlen) {

	unsigned char* ppB = dst;
	unsigned char* ppM = src;
	for (int i = 0; i < height; i++) {
		memcpy(ppB, ppM, width);
		ppB += (width);
		ppM += ((width + padlen));
	}
	for (int i = 0; i < height / 2; i++) {
		memcpy(ppB, ppM, width);
		ppB += (width);
		ppM += ((width + padlen));
	}

	return (ppB - dst);
}

int cpNV12_drop_1(unsigned char* dst, unsigned char* src, int width, int height, int padlen, int idrop) {

	unsigned char* ppB = dst;
	unsigned char* ppM = src;
	for (int i = 0; i < height - idrop; i++) {
		memcpy(ppB, ppM, width);
		ppB += (width);
		ppM += ((width + padlen));
	}
	ppM += ((width + padlen)*idrop);
	for (int i = 0; i < (height - idrop) / 2; i++) {
		memcpy(ppB, ppM, width);
		ppB += (width);
		ppM += ((width + padlen));
	}

	return (ppB - dst);
}

GstFlowReturn RtpTask::on_video_sample(GstElement* sink, gpointer ptr) {

	//puts("----  on video ---");
	RtpTask * pTask = (RtpTask*)ptr;
	GstSample* videoSample;

	g_signal_emit_by_name(sink, "pull-sample", &videoSample);

	while (videoSample) {
		GstBuffer* buf = gst_sample_get_buffer(videoSample);
		GstVideoMeta *meta = gst_buffer_get_video_meta(buf);
		guint nplanes = GST_VIDEO_INFO_N_PLANES(&(pTask->videoinfo_));
		GstVideoFormat pixfmt = GST_VIDEO_INFO_FORMAT(&(pTask->videoinfo_));
		const char *pixfmt_str = gst_video_format_to_string(pixfmt);

		GstMapInfo map_info;
		if (!gst_buffer_map(buf, &map_info, (GstMapFlags)GST_MAP_READ))
		{
			g_print("gst_buffer_map() error!---eos \n");
			break;
		}
		int source_width = GST_VIDEO_INFO_WIDTH(&(pTask->videoinfo_));
		int source_height = GST_VIDEO_INFO_HEIGHT(&(pTask->videoinfo_));

		//printf("1111----------video old:%d ,%d new £º%d ,%d----------\n", GST_BUFFER_PTS(buf), GST_BUFFER_DTS(buf));


		if (pTask->framecount_ == 0) {
			
			{
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

				pTask->pFlow_->src_fs_d_ = GST_VIDEO_INFO_FPS_D(&(pTask->videoinfo_));
				pTask->pFlow_->src_fs_n_ = GST_VIDEO_INFO_FPS_N(&(pTask->videoinfo_));

			}
			pTask->pFlow_->task_start_time_ = ts_time::current();
			pTask->pFlow_->pixel_fmt_ = pixfmt_str;
			DebugLog::writeLogF("===================================\n");
			DebugLog::writeLogF("rtp Task video stream information:\n");
			DebugLog::writeLogF("  taskid: %s\n", pTask->pFlow_->taskid_.c_str());
			DebugLog::writeLogF("  rtpurl: %s\n", pTask->pFlow_->rtspUrl_.c_str());
			DebugLog::writeLogF("  start time: %d\n", pTask->pFlow_->task_start_time_);
			DebugLog::writeLogF("  size: %u x %u pixel\n", source_width, source_height);
			DebugLog::writeLogF("  pixel format: %s  number of planes: %u\n", pixfmt_str, nplanes);
			//	printf("  video meta found: %d\n", (meta != NULL));
			DebugLog::writeLogF("  nvv frame size : %d \n", map_info.size);
			DebugLog::writeLogF("===================================\n\n");
		}

		if (pTask->cur_resolution_width_ == 0 || pTask->cur_resolution_hetight_ == 0) {
			pTask->cur_resolution_width_ = source_width;
			pTask->cur_resolution_hetight_ = source_height;
		}

		if (pTask->cur_resolution_width_ != source_height && pTask->cur_resolution_hetight_ != source_width) {
			if (pTask->sharebuf_ != NULL) {
				free(pTask->sharebuf_);
				pTask->sharebuf_ = NULL;
			}
		}

		//puts("------------------------one fram----------------------------");
		if (pTask->pFlow_->pRawData_in->size() < 5) {

			/*		puts("==================================one frame==============");
					FILE * yuvout = fopen("tstYuv0822.yuv", "w");
					fwrite(map_info.data, map_info.size, 1, yuvout);
					fclose(yuvout);
					exit(0);

			*/
			Packet* newData;
			
			if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV) {
				
				bool checked = false;
				if (map_info.size == GST_VIDEO_INFO_SIZE(&(pTask->videoinfo_))) {
					newData = new Packet(map_info.data, map_info.size);
					newData->size = map_info.size;
					newData->type_ = 1;//video
					newData->width = source_width;
					newData->height = source_height;
					checked = true;
				//	printf("-----nvv in to-------------w %d h %d- total %d\n", source_width, source_height, map_info.size);
				}
				else {
					int bufsize = map_info.size;
					int imgsize = GST_VIDEO_INFO_SIZE(&(pTask->videoinfo_));
					int tmpDropline = (bufsize - imgsize) * 2 / 3 / source_width;

					int tmpsize = (source_width)*(source_height + tmpDropline) * 3 / 2;
					if (tmpsize == bufsize) {
						checked = true;
						if (strcmp(pixfmt_str, "NV12") == 0) {
							int padlen = 0;
							unsigned char* pTmp = (unsigned char*)malloc(source_width * source_height * 3 / 2);
							int yuvsize = cpNV12_drop_1(pTmp, map_info.data, source_width, source_height + tmpDropline, padlen, tmpDropline);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = source_width;
							newData->height = source_height;
						}
						else {
							DebugLog::writeLogF("==============decode pixfmt=%d\n", pixfmt_str);
						}
					}
					else {
						checked = false;

					}
				}
				if (!checked) {
					printf("not handle res issue in nvcodec\n");
					if (map_info.size == 1850880) { // 1280*960
						int padlen = 0;
						unsigned char* pTmp = (unsigned char*)malloc(1280 * 966 * 3 / 2);
						int yuvsize = cpNV12_drop_1(pTmp, map_info.data, 1280, 966, padlen, 6);
						newData = new Packet(pTmp, yuvsize);
						newData->size = yuvsize;
						free(pTmp);

						newData->width = 1280;
						newData->height = 960;

					}
					else if (map_info.size == 1420800) { // 1280*720
						int padlen = 0;
						unsigned char* pTmp = (unsigned char*)malloc(1280 * 740 * 3 / 2);
						int yuvsize = cpNV12_drop_1(pTmp, map_info.data, 1280, 740, padlen, 20);
						newData = new Packet(pTmp, yuvsize);
						newData->size = yuvsize;
						free(pTmp);

						newData->width = 1280;
						newData->height = 720;

					}
					else if (map_info.size == 5544960) { // 2560*1440
						int padlen = 0;
						unsigned char* pTmp = (unsigned char*)malloc(2560 * 1444 * 3 / 2);
						int yuvsize = cpNV12_drop_1(pTmp, map_info.data, 2560, 1444, padlen, 4);
						newData = new Packet(pTmp, yuvsize);
						newData->size = yuvsize;
						free(pTmp);

						newData->width = 1560;
						newData->height = 1440;

					}
					else if (map_info.size == 4548096) { // 2304*1296
						int padlen = 0;
						unsigned char* pTmp = (unsigned char*)malloc(2304 * 1296 * 3 / 2);
						int yuvsize = cpNV12_drop_1(pTmp, map_info.data, 2304, 1316, padlen, 20);
						newData = new Packet(pTmp, yuvsize);
						newData->size = yuvsize;
						free(pTmp);

						newData->width = 1560;
						newData->height = 1440;

					}
				}
			}
			else
			//	printf("-----other in to--------------\n");
				if (1 || TaskManager::GetInstance()->chip_type_ == CHIP_ROCKCHIP || TaskManager::GetInstance()->chip_type_ == CHIP_SOFTWARE || TaskManager::GetInstance()->chip_type_ == CHIP_TRYCPU) {
					bool checked = false;
					if (map_info.size == GST_VIDEO_INFO_SIZE(&(pTask->videoinfo_))) {
						newData = new Packet(map_info.data, map_info.size);
						newData->size = map_info.size;
						newData->type_ = 1;//video
						newData->width = source_width;
						newData->height = source_height;
						checked = true;
					}
					else {
						int bufsize = map_info.size;
						int imgsize = GST_VIDEO_INFO_SIZE(&(pTask->videoinfo_));
						int tmpPadlen = (bufsize - imgsize) / (source_height * 3 / 2);
						int aMod = tmpPadlen % 32;
						int tmpsize = (source_width + tmpPadlen)*source_height * 3 / 2;
						if (tmpsize == bufsize) {
							checked = true;
							if (strcmp(pixfmt_str, "NV12") == 0) {
								int padlen = tmpPadlen;
								unsigned char* pTmp = (unsigned char*)malloc(source_width * source_height * 3 / 2);
								int yuvsize = cpNV12_2(pTmp, map_info.data, source_width, source_height, padlen);
								newData = new Packet(pTmp, yuvsize);
								newData->size = yuvsize;
								free(pTmp);
								newData->width = source_width;
								newData->height = source_height;
							}
							else {
								DebugLog::writeLogF("==============decode pixfmt=%d\n", pixfmt_str);
							}
						}
						else {
							checked = false;

						}

					}
					if (checked == false) {
						//3760128
						if (map_info.size == 353280) { // 640*360
							/*int padlen = 0;
							unsigned char* pTmp = (unsigned char*)malloc(640 * 368 * 3 / 2);
							int yuvsize = cpNV12_drop_1(pTmp, map_info.data, 640, 368, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);*/

							newData = new Packet(map_info.data, map_info.size);
							newData->size = map_info.size;
							newData->type_ = 1;//video

							newData->width = 640;
							newData->height = 368;

						}
						else if (map_info.size == 423936) { // 640*360
							int padlen = 128;
							unsigned char* pTmp = (unsigned char*)malloc(640 * 368 * 3 / 2);
							int yuvsize = cpNV12_drop_1(pTmp, map_info.data, 640, 368, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 640;
							newData->height = 360;

						}
						else if (map_info.size == 3760128) {
							int padlen = 384;
							unsigned char* pTmp = (unsigned char*)malloc(1920 * 1088 * 3 / 2);
							int yuvsize = cpNV12_drop_1(pTmp, map_info.data, 1920, 1088, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 1920;
							newData->height = 1080;

						}
						else if (map_info.size == 3133440 || map_info.size == 4177920) {// h264 16bit 1920 * 1080 == 1920 * 1088

							int padlen = 0;
							unsigned char* pTmp = (unsigned char*)malloc(1920 * 1080 * 3 / 2);
							int yuvsize = cpNV12_drop_1(pTmp, map_info.data, 1920, 1088, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 1920;
							newData->height = 1080;

						}
						else if (map_info.size == 8146944) {// 2880 * 1620 2880 * 1632
							int padlen = 448;
							unsigned char* pTmp = (unsigned char*)malloc(2880 * 1620 * 3 / 2);
							int yuvsize = cpNV12_2(pTmp, map_info.data, 2880, 1620, padlen);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);
							newData->width = 2880;
							newData->height = 1632;
						}
						else if (map_info.size == 1413120) {//h265 640*480 768 * 480 
							newData = new Packet(map_info.data, map_info.size);
							newData->size = map_info.size;
							newData->type_ = 1;//video
							newData->width = 1280;
							newData->height = 736;
						}
						else if (map_info.size == 1420800) {//h265 640*480 768 * 480 
							newData = new Packet(map_info.data, map_info.size);
							newData->size = map_info.size;
							newData->type_ = 1;//video
							newData->width = 1280;
							newData->height = 740;
						}
						//-------
						else if (map_info.size == 3732480 || map_info.size == 4976640) {// h265 256bit 1920 * 1080  == 2304 * 1080
							int padlen = 384;
							unsigned char* pTmp = (unsigned char*)malloc(1920 * 1080 * 3 / 2);
							int yuvsize = cpNV12_2(pTmp, map_info.data, 1920, 1080, padlen);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);
							newData->width = 1920;
							newData->height = 1080;
						}
						else if (map_info.size == 6082560 || map_info.size == 8110080) {// h265 2560 * 1440 256 2816 1584
							int padlen = 256;
							unsigned char* pTmp = (unsigned char*)malloc(2560 * 1440 * 3 / 2);
							int yuvsize = cpNV12_2(pTmp, map_info.data, 2560, 1440, padlen);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);
							newData->width = 2560;
							newData->height = 1440;
						}
						else if (map_info.size == 6420480) {//  2688 1520
							int padlen = 128;
							unsigned char* pTmp = (unsigned char*)malloc(2688 * 1520 * 3 / 2);
							int yuvsize = cpNV12_2(pTmp, map_info.data, 2688, 1520, padlen);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);
							newData->width = 2688;
							newData->height = 1520;
						}

						else if (map_info.size == 8626176) {// 3072 * 1728 
							int padlen = 256;
							unsigned char* pTmp = (unsigned char*)malloc(3072 * 1728 * 3 / 2);
							int yuvsize = cpNV12_2(pTmp, map_info.data, 3072, 1728, padlen);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);
							newData->width = 3072;
							newData->height = 1728;
						}
						else
						{
							newData = new Packet(map_info.data, map_info.size);
							newData->size = map_info.size;
							newData->type_ = 1;//video
							newData->width = source_width;
							newData->height = source_height;
						}
					}
				}

			newData->seq_ = pTask->cur_seq_++;
			newData->pts_ = GST_BUFFER_PTS(buf);
			pTask->pFlow_->pRawData_in->push(newData);
			//	printf("			++++++++++++++++++++++  new video ts :%ld  seq=%d\n", newData->pts_, newData->seq_);
				/*if (pTask->cur_seq_ > 3) {
					sleep(1);

					exit(0);
				}*/
		}
		gst_buffer_unmap(buf, &map_info);
		gst_sample_unref(videoSample);

		pTask->framecount_++;

		return GST_FLOW_OK;
	}

	return GST_FLOW_ERROR;
}

#define AUDIO_SAMP_BLK	640
GstFlowReturn RtpTask::on_audio_sample(GstElement* sink, gpointer ptr) {

	RtpTask * pTask = (RtpTask*)ptr;
	GstSample* audioSample;
	//	puts("=== on audio ===");


	g_signal_emit_by_name(sink, "pull-sample", &audioSample);

	if (audioSample)
	{
		GstBuffer* buf = gst_sample_get_buffer(audioSample);
		/*GstAudioMeta *meta = gst_buffer_get_audio_meta(buf);

		if (meta != NULL) {
			printf("audio sample:ch %d %d rt %d desc %s\n", meta->info.channels, meta->info.bpf, meta->info.rate, meta->info.finfo->description);
		}*/

		GstMapInfo map_info;
		if (!gst_buffer_map(buf, &map_info, (GstMapFlags)GST_MAP_READ))
		{
			g_print("gst_buffer_map() error!---eos \n");

		}

		if (pTask->framecount_ >= 0) {

			if (pTask->lastAudioStmp_ == 0) {
				pTask->lastAudioStmp_ = GST_BUFFER_PTS(buf);
			}

			if (pTask->pFlow_->pAudioData_in->size() < 50)
			{

				memcpy(pTask->shareAudiobuf_ + pTask->shareAudioLen, map_info.data, map_info.size);
				pTask->shareAudioLen += map_info.size;

				int blksize = pTask->shareAudioLen / AUDIO_SAMP_BLK;

				for (int i = 0; i < blksize; i++) {
					Packet* newData = new Packet(pTask->shareAudiobuf_ + i * AUDIO_SAMP_BLK, AUDIO_SAMP_BLK);
					newData->type_ = 2;//audio
					newData->size = AUDIO_SAMP_BLK;
					newData->pts_ = pTask->lastAudioStmp_ + 40 * 1000000;
					pTask->pFlow_->pAudioData_in->push(newData);

					pTask->shareAudioLen -= AUDIO_SAMP_BLK;
				}

				if (pTask->shareAudioLen > 0) {
					memmove(pTask->shareAudiobuf_, pTask->shareAudiobuf_ + blksize * AUDIO_SAMP_BLK, pTask->shareAudioLen);
				}
			}
			else
			{
				pTask->lastAudioStmp_ = GST_BUFFER_PTS(buf);
				puts("-------------- audio buff over load -------------------");
			}
		}
		else
		{
			if (ts_time::current() - pTask->pFlow_->task_start_time_ > 2) {
				if (pTask->needteardown_ != 1) {
					pTask->closestyle_ = 6;
					//gst_bus_post(pTask->bus_, gst_message_new_eos(GST_OBJECT_CAST(pTask->trans_layer_src_)));
					pTask->reportStatus();
					pTask->needteardown_ = 1;
				}
			}

		}

		//	printf("audio frame info----------%d\n", map_info.size);
		//printf("------audio pts info   %ld len=%d\n", GST_BUFFER_PTS(buf), map_info.size);

		gst_buffer_unmap(buf, &map_info);
		gst_sample_unref(audioSample);
	}

	return GST_FLOW_OK;


}

