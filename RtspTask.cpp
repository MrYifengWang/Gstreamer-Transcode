#include "RtspTask.h"
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

RtspTask::RtspTask(TaskFlow* pFlow)
{
	pFlow_ = pFlow;
	//strUrl_ = pFlow->rtspUrl_; //
	withaudio_ = pFlow->withaudio_;
	tid_ = TaskManager::total_tid++;
	gst_init(NULL, NULL);
	puts("create RtspTask");
}


RtspTask::~RtspTask() {
	puts("~delete RtspTask");
	//releasePileline();
	reportStatus();
	Packet* newData = new Packet();
	pFlow_->pRawData_in->push(newData);
	Packet* newData1 = new Packet();
	pFlow_->pAudioData_in->push(newData1);
	pFlow_->onComplete(2);
}

void RtspTask::reportStatus()
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
	else if (closestyle_ == 6) {
		root["code"] = ERR_SRC_AV_OUT_SYNC;
		root["message"] = "src rtsp audio and video is out sync, you need restart this task";

	}
	else if (closestyle_ == 7) {
		root["code"] = ERR_RESL_CHANGED;
		root["message"] = "camera's resolution changed";

	}
	else if (closestyle_ == 8) {
		root["code"] = ERR_FPS_CHANGED;
		root["oldfps"] = oldfps;
		root["fps"] = curfps;
		root["message"] = "camera's fps changed";

	}

	TaskManager::GetInstance()->onAnyWorkStatus(root, 0);

}
void *RtspTask::RtspTask::startThread(TaskFlow* pFlow)
{
	RtspTask* thread = new RtspTask(pFlow);

	thread->setAutoDelete(true);
	thread->start();

	return thread;
}
void RtspTask::stop(int code)
{
	//printf("stop rtsp task 1--------------------%s \n",  pFlow_->taskid_.c_str());
	closestyle_ = code;
	needteardown_ = 1;
	bool ret = gst_bus_post(bus_, gst_message_new_eos(GST_OBJECT_CAST(rtspsrc_)));
	printf("stop rtsp task--------------------%d %s \n", ret, pFlow_->taskid_.c_str());

}
bool RtspTask::RtspTask::onStart()
{
	return true;
}
void RtspTask::RtspTask::run()
{

	rtsp_rtp_transmode_ = TCP_CONN_MODE;
	isRun = STATUS_CONNECTING;

	DebugLog::writeLogF("start connect rtsp url: %s \n", pFlow_->rtspUrl_.c_str());


	int ret = 0;
	if (pFlow_->inputtype_ == "rtsp") {
		puts("-----------------rtsp src-------------------------");
		ret = buildPipeline();
		//	ret = buildUsbPipeline();
	}
	else if (pFlow_->inputtype_ == "rtmp") {
		ret = buildRtmpPipeline();
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

		do {
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

void RtspTask::myfps(int deltatm) {
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
		if (oldfps >0 && changeCount >= 3) {
			changeCount = 0;
			closestyle_ = 8;
			//reportStatus();
		}
		if (oldfps == 0) {
			oldfps = curfps;
		}
	}

}

static gboolean before_send_callback(GstElement * rtspsrc, GstRTSPMessage * msg, gpointer udata)
{
	RtspTask *pTask = (RtspTask *)udata;
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
void RtspTask::on_sdp_callback(GstElement * rtspsrc, GstSDPMessage * sdp, gpointer udata)
{
	//gst_sdp_message_dump(sdp);
	RtspTask *pTask = (RtspTask *)udata;

	int count = gst_sdp_message_medias_len(sdp);
	for (int i = 0; i < count; i++) {
		const GstSDPMedia * tmp = gst_sdp_message_get_media(sdp, i);
		//	printf("-----sdp media type = %s\n", gst_sdp_media_get_media(tmp));
		if (strcmp(gst_sdp_media_get_media(tmp), "audio") == 0) {
			pTask->pFlow_->haveaudio_ = 1;
		}
	}

}

gboolean RtspTask::select_stream_callback(GstElement * rtspsrc, guint num, GstCaps * caps, gpointer udata) {

	GstUtil::print_caps(caps, "RtspTask");
	return true;

}
int RtspTask::buildPipeline() {

	// Build Pipeline 
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	rtspsrc_ = gst_element_factory_make("rtspsrc", ("rtspsrc" + std::to_string(tid_)).c_str());
	videodecoder_ = gst_element_factory_make("decodebin", ("videodecodebin" + std::to_string(tid_)).c_str());
	videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	appvideosink_ = gst_element_factory_make("appsink", ("videoapp_sink" + std::to_string(tid_)).c_str());
	cpasfilter_ = gst_element_factory_make("capsfilter", NULL);
	if (!cpasfilter_) {
		puts("--- caps filter created failed ---");
	}

	gst_util_set_object_arg(G_OBJECT(cpasfilter_), "caps",
		"video/x-raw, "
		"format={  NV12}");

	//"format={ I420, YV12, YUY2, UYVY, AYUV, Y41B, Y42B, YVYU, Y444, v210, v216, NV12, NV21, UYVP, A420, YUV9, YVU9, IYU1 }");




	if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV || TaskManager::GetInstance()->chip_type_ == CHIP_TRYCPU)
	{
		vconverter_ = gst_element_factory_make("nvvideoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	}
	else {
		vconverter_ = gst_element_factory_make("videoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
		//vconverter_ = gst_element_factory_make("nvvideoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	}


	/*if (!pipeline_ || !flvdemux_ || !rtspsrc_ | !videodecoder_ || !videoqueue_ || !appvideosink_ || !cpasfilter_) {
		g_printerr("One element could not be created 1.\n");
		return -1;
	}*/

	if (!pipeline_ || !rtspsrc_ | !videodecoder_ || !vconverter_ || !cpasfilter_ || !videoqueue_ || !appvideosink_) {
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
	g_signal_connect(G_OBJECT(appvideosink_), "new-sample", G_CALLBACK(RtspTask::on_video_sample), this);


	//gst_pad_add_probe(gst_element_get_static_pad(rtspsrc_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe2, this, NULL);
	//decode select
	/*g_signal_connect(G_OBJECT(videodecoder_), "autoplug-continue", G_CALLBACK(RtspTask::autoplug_continue_callback), this);
	g_signal_connect(G_OBJECT(videodecoder_), "autoplug-select", G_CALLBACK(RtspTask::autoplug_select_callback), this);*/


	EdUrlParser* url = EdUrlParser::parseRtspUrl(pFlow_->rtspUrl_);
	/*vector<string> paths;
	EdUrlParser::parsePath(&paths, url->path);
	for (int i = 0; i < paths.size(); i++) {
		DebugLog::writeLogF("----path part: %d %s \n", i, paths[i].c_str());
	}*/

	printf("host=%s\n user=%s   pswd= %s path =%s  \n\n", url->hostName.c_str(), url->authUser_.c_str(), url->authPswd_.c_str(), url->path.c_str());

	//rtsp setting;
	g_object_set(GST_OBJECT(rtspsrc_), "location", pFlow_->rtspUrl_.c_str(), NULL);
	g_object_set(GST_OBJECT(rtspsrc_), "latency", pFlow_->delay_input_, NULL);
	g_object_set(GST_OBJECT(rtspsrc_), "debug", true, NULL);
	g_object_set(G_OBJECT(rtspsrc_), "timeout", 3000, NULL);

	if (!url->authUser_.empty()) {
		g_object_set(GST_OBJECT(rtspsrc_), "user-id", url->authUser_.c_str(), NULL);
		g_object_set(GST_OBJECT(rtspsrc_), "user-pw", url->authPswd_.c_str(), NULL);
	}
	//g_object_set(GST_OBJECT(rtspsrc_), "probation ", 10, NULL);

	g_object_set(GST_OBJECT(rtspsrc_), "short-header", true, NULL);

	//g_object_set(GST_OBJECT(rtspsrc_), "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);

	if (pFlow_->rtspUrl_.find("proto=Onvif") != std::string::npos && pFlow_->rtspUrl_.find("realmonitor") != std::string::npos) {

	}
	else {

		if (rtsp_rtp_transmode_ == 1) {
			g_object_set(GST_OBJECT(rtspsrc_), "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
		}
		else if (rtsp_rtp_transmode_ == 2) {
			g_object_set(GST_OBJECT(rtspsrc_), "protocols", GST_RTSP_LOWER_TRANS_UDP, NULL);
		}
	}


	g_object_set(G_OBJECT(videoqueue_), "max-size-buffers", this->pFlow_->buf_input_, NULL);
	g_object_set(G_OBJECT(videoqueue_), "leaky", 2, NULL);
	gst_app_sink_set_max_buffers(GST_APP_SINK(appvideosink_), 1);
	gst_app_sink_set_buffer_list_support(GST_APP_SINK(appvideosink_), FALSE);

	//Partially static pipeline link
	gst_bin_add_many(GST_BIN(pipeline_), rtspsrc_, videodecoder_, vconverter_, cpasfilter_, videoqueue_, appvideosink_, NULL);

	/*if (addAudioBranch() < 0) {
		return -1;
	}*/

	if (!gst_element_link_many(vconverter_, cpasfilter_, videoqueue_, appvideosink_, NULL)) {
		g_printerr("appsink Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}
	//g_signal_connect(G_OBJECT(videodecoder_), "autoplug-continue", G_CALLBACK(RtspTask::autoplug_continue_callback), this);
	//g_signal_connect(G_OBJECT(videodecoder_), "autoplug-select", G_CALLBACK(RtspTask::autoplug_select_callback), this);
	//Dynamic link
	g_signal_connect(rtspsrc_, "pad-added", G_CALLBACK(on_rtsp2dec_link), this);
	g_signal_connect(videodecoder_, "pad-added", G_CALLBACK(on_dec2conv_link), this);

	//g_signal_connect(rtspsrc, "select-stream", G_CALLBACK(RtspTask::select_stream_callback), this);
	g_signal_connect(rtspsrc_, "on-sdp", G_CALLBACK(RtspTask::on_sdp_callback), this);
	g_signal_connect(rtspsrc_, "before-send", G_CALLBACK(before_send_callback), this);

	// start pipeline
	/*bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
	gst_bus_add_watch(bus_, RtspTask::onBusMessage, this);
	gst_element_set_state(GST_ELEMENT(pipeline_), GST_STATE_PLAYING);*/

	return 0;
}


int RtspTask::buildPipeline1() {

	// Build Pipeline 
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	rtspsrc_ = gst_element_factory_make("rtspsrc", ("rtspsrc" + std::to_string(tid_)).c_str());
	rtpdepay_ = gst_element_factory_make("rtph264depay", ("rtpdepay11" + std::to_string(tid_)).c_str());
	h26Xparse_ = gst_element_factory_make("h264parse", ("rtppayser11" + std::to_string(tid_)).c_str());
	videodecoder_ = gst_element_factory_make("decodebin", ("videodecodebin" + std::to_string(tid_)).c_str());
	videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	appvideosink_ = gst_element_factory_make("appsink", ("videoapp_sink" + std::to_string(tid_)).c_str());
	cpasfilter_ = gst_element_factory_make("capsfilter", NULL);
	if (!cpasfilter_) {
		puts("--- caps filter created failed ---");
	}

	gst_util_set_object_arg(G_OBJECT(cpasfilter_), "caps",
		"video/x-raw, "
		"format={  NV12}");

	//"format={ I420, YV12, YUY2, UYVY, AYUV, Y41B, Y42B, YVYU, Y444, v210, v216, NV12, NV21, UYVP, A420, YUV9, YVU9, IYU1 }");




	if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV || TaskManager::GetInstance()->chip_type_ == CHIP_TRYCPU)
	{
		vconverter_ = gst_element_factory_make("nvvideoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	}
	else {
		vconverter_ = gst_element_factory_make("videoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
		//vconverter_ = gst_element_factory_make("nvvideoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	}


	/*if (!pipeline_ || !flvdemux_ || !rtspsrc_ | !videodecoder_ || !videoqueue_ || !appvideosink_ || !cpasfilter_) {
		g_printerr("One element could not be created 1.\n");
		return -1;
	}*/

	if (!pipeline_ || !rtspsrc_ | !videodecoder_ || !vconverter_ || !cpasfilter_ || !videoqueue_ || !appvideosink_) {
		g_printerr("One element could not be created 2.\n");
		return -1;
	}

	// appsink setting
	g_object_set(G_OBJECT(appvideosink_), "sync", FALSE, NULL);
	GstPad* apppad = gst_element_get_static_pad(appvideosink_, "sink");
	gst_pad_add_probe(apppad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, appsink_query_cb, NULL, NULL);
	gst_object_unref(apppad);
	gst_base_sink_set_max_lateness(GST_BASE_SINK(appvideosink_), 70 * GST_MSECOND);
	gst_base_sink_set_qos_enabled(GST_BASE_SINK(appvideosink_), TRUE);
	g_object_set(G_OBJECT(appvideosink_), "max-buffers", this->pFlow_->buf_input_ / 3, NULL);
	gst_pad_add_probe(gst_element_get_static_pad(appvideosink_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe, this, NULL);
	g_object_set(G_OBJECT(appvideosink_), "emit-signals", TRUE, NULL);
	g_signal_connect(G_OBJECT(appvideosink_), "new-sample", G_CALLBACK(RtspTask::on_video_sample), this);

	gst_pad_add_probe(gst_element_get_static_pad(videodecoder_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe2, this, NULL);

	//gst_pad_add_probe(gst_element_get_static_pad(rtspsrc_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe2, this, NULL);
	//decode select
//	g_signal_connect(G_OBJECT(videodecoder_), "autoplug-factories", G_CALLBACK(RtspTask::autoplug_continue_callback), this);
//	g_signal_connect(G_OBJECT(videodecoder_), "unknown-type", G_CALLBACK(RtspTask::autoplug_continue_callback), this);
//	g_signal_connect(G_OBJECT(videodecoder_), "autoplug-continue", G_CALLBACK(RtspTask::autoplug_continue_callback), this);
	//g_signal_connect(G_OBJECT(videodecoder_), "autoplug-select", G_CALLBACK(RtspTask::autoplug_select_callback), this);


	EdUrlParser* url = EdUrlParser::parseRtspUrl(pFlow_->rtspUrl_);
	/*vector<string> paths;
	EdUrlParser::parsePath(&paths, url->path);
	for (int i = 0; i < paths.size(); i++) {
		DebugLog::writeLogF("----path part: %d %s \n", i, paths[i].c_str());
	}*/

	printf("host=%s\n user=%s   pswd= %s path =%s  \n\n", url->hostName.c_str(), url->authUser_.c_str(), url->authPswd_.c_str(), url->path.c_str());

	//rtsp setting;
	g_object_set(GST_OBJECT(rtspsrc_), "location", pFlow_->rtspUrl_.c_str(), NULL);
	g_object_set(GST_OBJECT(rtspsrc_), "latency", pFlow_->delay_input_, NULL);
	g_object_set(GST_OBJECT(rtspsrc_), "debug", true, NULL);
	g_object_set(G_OBJECT(rtspsrc_), "timeout", 3000, NULL);

	if (!url->authUser_.empty()) {
		g_object_set(GST_OBJECT(rtspsrc_), "user-id", url->authUser_.c_str(), NULL);
		g_object_set(GST_OBJECT(rtspsrc_), "user-pw", url->authPswd_.c_str(), NULL);
	}
	//g_object_set(GST_OBJECT(rtspsrc_), "probation ", 10, NULL);

	g_object_set(GST_OBJECT(rtspsrc_), "short-header", true, NULL);

	//g_object_set(GST_OBJECT(rtspsrc_), "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);

	if (pFlow_->rtspUrl_.find("proto=Onvif") != std::string::npos && pFlow_->rtspUrl_.find("realmonitor") != std::string::npos) {

	}
	else {

		if (rtsp_rtp_transmode_ == 1) {
			g_object_set(GST_OBJECT(rtspsrc_), "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
		}
		else if (rtsp_rtp_transmode_ == 2) {
			g_object_set(GST_OBJECT(rtspsrc_), "protocols", GST_RTSP_LOWER_TRANS_UDP, NULL);
		}
	}


	g_object_set(G_OBJECT(videoqueue_), "max-size-buffers", this->pFlow_->buf_input_, NULL);
	g_object_set(G_OBJECT(videoqueue_), "leaky", 2, NULL);
	gst_app_sink_set_max_buffers(GST_APP_SINK(appvideosink_), 1);
	gst_app_sink_set_buffer_list_support(GST_APP_SINK(appvideosink_), FALSE);

	//Partially static pipeline link
	gst_bin_add_many(GST_BIN(pipeline_), rtspsrc_, rtpdepay_, h26Xparse_,videodecoder_, vconverter_, cpasfilter_, videoqueue_, appvideosink_, NULL);

	/*if (addAudioBranch() < 0) {
		return -1;
	}*/


	if (!gst_element_link_many(rtpdepay_, h26Xparse_, videodecoder_,  NULL)) {
		g_printerr("appsink Elements could not be linked 1.\n");
		gst_object_unref(pipeline_);
		return -1;
	}
	if (!gst_element_link_many(   vconverter_, cpasfilter_, videoqueue_, appvideosink_, NULL)) {
		g_printerr("appsink Elements could not be linked 2.\n");
		gst_object_unref(pipeline_);
		return -1;
	}
	//g_signal_connect(G_OBJECT(videodecoder_), "autoplug-continue", G_CALLBACK(RtspTask::autoplug_continue_callback), this);
	//g_signal_connect(G_OBJECT(videodecoder_), "autoplug-select", G_CALLBACK(RtspTask::autoplug_select_callback), this);
	//Dynamic link
	g_signal_connect(rtspsrc_, "pad-added", G_CALLBACK(on_rtsp2demux_link), this);
	g_signal_connect(videodecoder_, "pad-added", G_CALLBACK(on_dec2conv_link), this);

	//g_signal_connect(rtspsrc, "select-stream", G_CALLBACK(RtspTask::select_stream_callback), this);
	g_signal_connect(rtspsrc_, "on-sdp", G_CALLBACK(RtspTask::on_sdp_callback), this);
	g_signal_connect(rtspsrc_, "before-send", G_CALLBACK(before_send_callback), this);

	// start pipeline
	/*bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
	gst_bus_add_watch(bus_, RtspTask::onBusMessage, this);
	gst_element_set_state(GST_ELEMENT(pipeline_), GST_STATE_PLAYING);*/

	return 0;
}


int RtspTask::buildUsbPipeline() {

	// Build Pipeline 
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	rtspsrc_ = gst_element_factory_make("v4l2src", ("rtspsrc" + std::to_string(tid_)).c_str());
	//videodecoder_ = gst_element_factory_make("nvv4l2decoder", ("videodecodebin" + std::to_string(tid_)).c_str());
	videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	appvideosink_ = gst_element_factory_make("appsink", ("videoapp_sink" + std::to_string(tid_)).c_str());
	cpasfilter_ = gst_element_factory_make("capsfilter", NULL);
	if (!cpasfilter_) {
		puts("--- caps filter created failed ---");
	}

	gst_util_set_object_arg(G_OBJECT(cpasfilter_), "caps",
		"video/x-raw, "
		"format={  NV12}");


	vconverter_ = gst_element_factory_make("nvvideoconvert", ("vconvertr" + std::to_string(tid_)).c_str());


	if (!pipeline_ || !rtspsrc_ || !cpasfilter_ || !videoqueue_ || !appvideosink_) {
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
	g_signal_connect(G_OBJECT(appvideosink_), "new-sample", G_CALLBACK(RtspTask::on_video_sample), this);

	g_object_set(GST_OBJECT(rtspsrc_), "device", "/dev/video0", NULL);

	g_object_set(G_OBJECT(videoqueue_), "max-size-buffers", this->pFlow_->buf_input_, NULL);
	g_object_set(G_OBJECT(videoqueue_), "leaky", 2, NULL);
	gst_app_sink_set_max_buffers(GST_APP_SINK(appvideosink_), 1);
	gst_app_sink_set_buffer_list_support(GST_APP_SINK(appvideosink_), FALSE);

	//Partially static pipeline link
	gst_bin_add_many(GST_BIN(pipeline_), rtspsrc_, vconverter_, cpasfilter_, videoqueue_, appvideosink_, NULL);

	/*if (addAudioBranch() < 0) {
		return -1;
	}*/

	if (!gst_element_link_many(rtspsrc_, vconverter_, cpasfilter_, videoqueue_, appvideosink_, NULL)) {
		g_printerr("appsink Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	return 0;
}

int RtspTask::buildRtmpPipeline() {

	// Build Pipeline 
	puts("start----------rtmp pipeline");
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	rtspsrc_ = gst_element_factory_make("rtmp2src", ("rtspsrc" + std::to_string(tid_)).c_str());
	flvdemux_ = gst_element_factory_make("flvdemux", ("demux" + std::to_string(tid_)).c_str());
	videodecoder_ = gst_element_factory_make("decodebin", ("videodecodebin" + std::to_string(tid_)).c_str());
	videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	appvideosink_ = gst_element_factory_make("appsink", ("videoapp_sink" + std::to_string(tid_)).c_str());
	cpasfilter_ = gst_element_factory_make("capsfilter", NULL);

	gst_util_set_object_arg(G_OBJECT(cpasfilter_), "caps",
		"video/x-raw, "
		"format={ I420, YV12, YUY2, UYVY, AYUV, Y41B, Y42B, "
		"YVYU, Y444, v210, v216, NV12, NV21, UYVP, A420, YUV9, YVU9, IYU1 }");



	if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV || TaskManager::GetInstance()->chip_type_ == CHIP_TRYCPU)
	{
		vconverter_ = gst_element_factory_make("nvvideoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	}
	else
		vconverter_ = gst_element_factory_make("videoconvert", ("vconvertr" + std::to_string(tid_)).c_str());


	if (!pipeline_ || !flvdemux_ || !rtspsrc_ | !videodecoder_ || !videoqueue_ || !appvideosink_ || !cpasfilter_) {
		g_printerr("One element could not be created 3.\n");
		return -1;
	}

	if (!pipeline_ || !flvdemux_ || !rtspsrc_ | !videodecoder_ || !videoqueue_ || !appvideosink_) {
		g_printerr("One element could not be created 4.\n");
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
	g_signal_connect(G_OBJECT(appvideosink_), "new-sample", G_CALLBACK(RtspTask::on_video_sample), this);



	//rtmp setting;

	EdUrlParser* url = EdUrlParser::parseUrl(pFlow_->rtspUrl_);
	DebugLog::writeLogF("host=%s\n port=%d\n path=%s\n\n", url->hostName.c_str(), 1935, url->path.c_str());
	vector<string> paths;
	EdUrlParser::parsePath(&paths, url->path);
	for (int i = 0; i < paths.size(); i++) {
		DebugLog::writeLogF("path part: %d %s \n", i, paths[i].c_str());
	}
	g_object_set(GST_OBJECT(rtspsrc_), "host", url->hostName.c_str(), NULL);
	g_object_set(GST_OBJECT(rtspsrc_), "port", 1935, NULL);
	if (paths.size() == 1) {
		g_object_set(GST_OBJECT(rtspsrc_), "application", "", NULL);
	}
	if (paths.size() > 0) {
		g_object_set(GST_OBJECT(rtspsrc_), "stream", paths[paths.size() - 1].c_str(), NULL);
	}
	if (paths.size() > 1) {
		g_object_set(GST_OBJECT(rtspsrc_), "location", pFlow_->rtspUrl_.c_str(), NULL);
	}


	if (url != NULL) // ==> make sure to free url object allocated by EdUrlParser
		delete url;
	/*if (rtsp_rtp_transmode_ == 1) {
		g_object_set(GST_OBJECT(rtspsrc_), "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
	}
	else if (rtsp_rtp_transmode_ == 2) {
		g_object_set(GST_OBJECT(rtspsrc_), "protocols", GST_RTSP_LOWER_TRANS_UDP, NULL);
	}*/


	g_object_set(G_OBJECT(videoqueue_), "max-size-buffers", this->pFlow_->buf_input_, NULL);
	g_object_set(G_OBJECT(videoqueue_), "leaky", 2, NULL);
	gst_app_sink_set_max_buffers(GST_APP_SINK(appvideosink_), 1);
	gst_app_sink_set_buffer_list_support(GST_APP_SINK(appvideosink_), FALSE);

	//Partially static pipeline link
	gst_bin_add_many(GST_BIN(pipeline_), rtspsrc_, flvdemux_, videodecoder_, vconverter_, cpasfilter_, videoqueue_, appvideosink_, NULL);
	if (!gst_element_link_many(rtspsrc_, videodecoder_, NULL)) {
		g_printerr("rtspsrc_ Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	if (!gst_element_link_many(vconverter_, cpasfilter_, videoqueue_, appvideosink_, NULL)) {
		g_printerr("appsink Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	//g_signal_connect(flvdemux_, "pad-added", G_CALLBACK(on_flv2dec_link), this);
	g_signal_connect(videodecoder_, "pad-added", G_CALLBACK(on_dec2conv_link1), this);
	/*if (addAudioBranch() < 0) {
		return -1;
	}

	if (!gst_element_link_many(vconverter_, videoqueue_, appvideosink_, NULL)) {
		g_printerr("appsink Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	//Dynamic link
	g_signal_connect(rtspsrc_, "pad-added", G_CALLBACK(on_rtsp2dec_link), this);
	g_signal_connect(videodecoder_, "pad-added", G_CALLBACK(on_dec2conv_link), this);
	*/

	return 0;
}
int RtspTask::addFakeBranch() {
	fakesink_ = gst_element_factory_make("fakesink", ("appdatasink_" + std::to_string(tid_)).c_str());
	gst_bin_add_many(GST_BIN(pipeline_), fakesink_, NULL);
	if (!gst_element_sync_state_with_parent(fakesink_))
	{
		g_printerr("sync fake element stat faile 1.\n");
	}
	return 0;
}


int RtspTask::addAudioBranch()
{
	if (withaudio_ > 0) {
		aconverter_ = gst_element_factory_make("audioconvert", ("aconv" + std::to_string(tid_)).c_str());
		audioqueue_ = gst_element_factory_make("queue", ("queue_fakesink" + std::to_string(tid_)).c_str());
		appaudiosink_ = gst_element_factory_make("appsink", ("appaudiosink_" + std::to_string(tid_)).c_str());
		audiodecoder_ = gst_element_factory_make("decodebin", ("adecodebin" + std::to_string(tid_)).c_str());
		if (!aconverter_ || !audioqueue_ || !audiodecoder_ || !appaudiosink_) {
			g_printerr("rtsp audio element could not be created.\n");
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
		g_signal_connect(G_OBJECT(appaudiosink_), "new-sample", G_CALLBACK(RtspTask::on_audio_sample), this);
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

		shareAudiobuf_ = (char *)malloc(20480);

	}

	return 0;

}

// destroy
void RtspTask::releasePileline()
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
		puts("============free shareAudiobuf_");
		shareAudiobuf_ = NULL;
	}

	isRun = STATUS_DISCONNECT;
}

GstPadProbeReturn RtspTask::pad_probe2(GstPad* pad, GstPadProbeInfo* info, gpointer user_data)
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

GstPadProbeReturn RtspTask::pad_probe(GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
	RtspTask *pTask = (RtspTask *)user_data;
	GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
	GstCaps *caps;


	(void)pad;

	if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
		return GST_PAD_PROBE_OK;

	gst_event_parse_caps(event, &caps);
	printf("=====================pad_probe=======================%d\n", GST_EVENT_TYPE(event));

	if (!caps) {
		GST_ERROR("caps event without caps");
		return GST_PAD_PROBE_OK;
	}

	if (!gst_video_info_from_caps(&pTask->videoinfo_, caps)) {
		GST_ERROR("caps event with invalid video caps");
		return GST_PAD_PROBE_OK;
	}


	/*int pfsd, fpsn;
	pfsd = GST_VIDEO_INFO_FPS_D(&(pTask->videoinfo_));
	fpsn = GST_VIDEO_INFO_FPS_N(&(pTask->videoinfo_));
	if (fpsn == 0 || fpsn / fpsd > 30) {
		need_measure_fps_ = 1;
	}*/
	//printf(">>>>>>>>>>>>>>>>>>  fps from caps: %d / %d = %d",fpsn,fpsd,fpsn/fpsd);
	//	puts("---- >>>>>>>>>>>>>>>> vidoe info\n");
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
GstPadProbeReturn RtspTask::pad_probe1(GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
	RtspTask *pTask = (RtspTask *)user_data;
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


void RtspTask::on_dec2conv_link(GstElement *element, GstPad *pad, gpointer data) {
	// GstPad *sinkpad;
	RtspTask *pTask = (RtspTask *)data;
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
		GstPad * tee_sinkpad = gst_element_get_static_pad(pTask->vconverter_, "sink");
		if (gst_pad_is_linked(tee_sinkpad)) {
			g_print("already linked. Ignoring.\n");
			gst_object_unref(tee_sinkpad);
		}
		gst_pad_link(pad, tee_sinkpad);
		gst_object_unref(tee_sinkpad);
	}

	gst_caps_unref(new_pad_caps);

}

void RtspTask::on_dec2conv_link1(GstElement *element, GstPad *pad, gpointer data) {
	// GstPad *sinkpad;
	puts("new pad from rtmp decoding");
	RtspTask *pTask = (RtspTask *)data;
	g_print("---video decoder received new pad: '%s' from '%s':\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(element));

	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;

	new_pad_caps = gst_pad_get_current_caps(pad);
	new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
	new_pad_type = gst_structure_get_name(new_pad_struct);
	DebugLog::writeLogF("++++++++++++++++++++ pad type: %s\n", new_pad_type);
	if (g_str_has_prefix(new_pad_type, "audio/x-raw")) {
		gst_caps_unref(new_pad_caps);
		return;

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
		GstPad * tee_sinkpad = gst_element_get_static_pad(pTask->vconverter_, "sink");
		if (gst_pad_is_linked(tee_sinkpad)) {
			g_print("already linked. Ignoring.\n");
			gst_object_unref(tee_sinkpad);
		}
		gst_pad_link(pad, tee_sinkpad);
		gst_object_unref(tee_sinkpad);
	}

	gst_caps_unref(new_pad_caps);

}

GstPadProbeReturn RtspTask::cb_have_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
	RtspTask *pTask = (RtspTask *)user_data;

	GstBuffer *buffer;

	buffer = GST_PAD_PROBE_INFO_BUFFER(info);

	if (pTask->cur_video_pts_ != buffer->pts) {
		DebugLog::writeLogF("=======    one video frame from rtp  %d  %d\n", buffer->pts, ts_time::currentpts(pTask->pFlow_->task_start_time_));
		pTask->cur_video_pts_ = buffer->pts;
	}

	//	buffer = gst_buffer_make_writable(buffer);
	if (buffer == NULL)
		return GST_PAD_PROBE_OK;
	//	GST_PAD_PROBE_INFO_DATA(info) = buffer;

	return GST_PAD_PROBE_OK;
}

void RtspTask::on_flv2dec_link(GstElement *element, GstPad *pad, gpointer data)
{
	puts("===============on_flv2dec_link===========================");

}


void RtspTask::on_rtsp2dec_link(GstElement* element, GstPad* pad, gpointer data) {

	RtspTask* pTask = (RtspTask*)data;

	/* Retrieve negotiated caps (or acceptable caps if negotiation is not finished yet) */

	g_print("----decodebin----Received new pad '%s' from '%s':\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(element));
	string tmpstr = GST_PAD_NAME(pad);
	int pos = tmpstr.rfind('_');
	tmpstr = tmpstr.substr(pos + 1);
	int payloadtype = atoi(tmpstr.c_str());
	int mediatype = GstUtil::checkMediaPtype(pad);
	//if (mediatype !=1 &&(mediatype == 0 || payloadtype == 96 || payloadtype == 97 || payloadtype == 98)) {
	if (mediatype != 1 && (mediatype == 0 || payloadtype == 96 || payloadtype == 97 || payloadtype == 98)) {
		GstPad* decode_sinkpad = gst_element_get_static_pad(pTask->videodecoder_, "sink");
		if (gst_pad_is_linked(decode_sinkpad)) {
			g_print("video rtsp-decoder already linked. Ignoring.\n");
			gst_object_unref(decode_sinkpad);
			return;

		}
		pTask->is_video_added = 1;
		if (GST_PAD_LINK_OK != gst_pad_link(pad, decode_sinkpad))
		{
			g_print("rtsp---videodecoder failed 2.\n");

		}
		//	gst_pad_add_probe(decode_sinkpad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)cb_have_data, data, NULL);
		//gst_object_unref(decode_sinkpad);
	}
	else if (payloadtype == 107) {
		pTask->addFakeBranch();
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
		puts("----link---- app");
	}
	else if (mediatype == 1) {
		puts("----link---- audio");
		if (pTask->withaudio_ <= 0 || pTask->is_audio_added == 1)
			return;
		pTask->is_audio_added = 1;
		pTask->addAudioBranch();
#if defined(NO_ENHANCE)
		//if (!gst_element_link_many(pTask->aconverter_, pTask->audioqueue_, pTask->appaudiosink_, NULL)) {
		if (!gst_element_link_many(pTask->aconverter_, pTask->audiorate_, pTask->audioresamp_, pTask->audiocpasfilter_, pTask->audioqueue_, pTask->appaudiosink_, NULL)) {
			g_printerr("audio branch Elements could not be linked.\n");
			gst_object_unref(pTask->pipeline_);
			return;
		}
#else
		if (!gst_element_link_many(pTask->aconverter_, pTask->audioresamp_, pTask->audiocpasfilter_, pTask->audioqueue_, pTask->appaudiosink_, NULL)) {
			g_printerr("audio branch Elements could not be linked.\n");
			gst_object_unref(pTask->pipeline_);
			return;
		}
#endif
		GstPad* decode_sinkpad = gst_element_get_static_pad(pTask->audiodecoder_, "sink");
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
}

void RtspTask::on_rtsp2demux_link(GstElement *element, GstPad *pad, gpointer data) {

	RtspTask *pTask = (RtspTask *)data;

	/* Retrieve negotiated caps (or acceptable caps if negotiation is not finished yet) */

	g_print("----decodebin----Received new pad '%s' from '%s':\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(element));
	string tmpstr = GST_PAD_NAME(pad);
	int pos = tmpstr.rfind('_');
	tmpstr = tmpstr.substr(pos + 1);
	int payloadtype = atoi(tmpstr.c_str());
	int mediatype = GstUtil::checkMediaPtype(pad);
	//if (mediatype !=1 &&(mediatype == 0 || payloadtype == 96 || payloadtype == 97 || payloadtype == 98)) {
	if (mediatype != 1 && (mediatype == 0 || payloadtype == 96 || payloadtype == 97 || payloadtype == 98)) {
		GstPad * decode_sinkpad = gst_element_get_static_pad(pTask->rtpdepay_, "sink");
		if (gst_pad_is_linked(decode_sinkpad)) {
			g_print("video rtsp-decoder already linked. Ignoring.\n");
			gst_object_unref(decode_sinkpad);
			return;

		}
		pTask->is_video_added = 1;
		if (GST_PAD_LINK_OK != gst_pad_link(pad, decode_sinkpad))
		{
			g_print("rtsp---videodecoder failed 1.\n");

		}
		//	gst_pad_add_probe(decode_sinkpad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)cb_have_data, data, NULL);
		//gst_object_unref(decode_sinkpad);
	}
	else if (payloadtype == 107) {
		pTask->addFakeBranch();
		GstPad * decode_sinkpad = gst_element_get_static_pad(pTask->fakesink_, "sink");
		if (gst_pad_is_linked(decode_sinkpad)) {
			g_print("app rtsp-decoder already linked. Ignoring.\n");
			gst_object_unref(decode_sinkpad);
			return;
		}
		if (GST_PAD_LINK_OK != gst_pad_link(pad, decode_sinkpad))
		{
			g_print("rtsp---app data sink failed.\n");
		}
		puts("----link---- app");
	}
	else if (mediatype == 1) {
		puts("----link---- audio");
		if (pTask->withaudio_ <= 0 || pTask->is_audio_added == 1)
			return;
		pTask->is_audio_added = 1;
		pTask->addAudioBranch();
#if defined(NO_ENHANCE)
		//if (!gst_element_link_many(pTask->aconverter_, pTask->audioqueue_, pTask->appaudiosink_, NULL)) {
		if (!gst_element_link_many(pTask->aconverter_, pTask->audiorate_, pTask->audioresamp_, pTask->audiocpasfilter_, pTask->audioqueue_, pTask->appaudiosink_, NULL)) {
			g_printerr("audio branch Elements could not be linked.\n");
			gst_object_unref(pTask->pipeline_);
			return;
		}
#else
		if (!gst_element_link_many(pTask->aconverter_, pTask->audioresamp_, pTask->audiocpasfilter_, pTask->audioqueue_, pTask->appaudiosink_, NULL)) {
			g_printerr("audio branch Elements could not be linked.\n");
			gst_object_unref(pTask->pipeline_);
			return;
		}
#endif
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
}

// appsink query
GstPadProbeReturn RtspTask::appsink_query_cb(GstPad * pad G_GNUC_UNUSED, GstPadProbeInfo * info, gpointer user_data G_GNUC_UNUSED)
{
	GstQuery *query = (GstQuery *)info->data;

	if (GST_QUERY_TYPE(query) != GST_QUERY_ALLOCATION)
		return GST_PAD_PROBE_OK;

	gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);

	return GST_PAD_PROBE_HANDLED;
}

// bus
gboolean RtspTask::onBusMessage(GstBus * bus, GstMessage * msg, gpointer user_data)
{
	RtspTask *pTask = (RtspTask *)user_data;


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
// rtsp init
gboolean RtspTask::autoplug_continue_callback(GstElement * bin, GstPad * pad, GstCaps * caps, gpointer udata)
{
//	GstUtil::print_caps(caps, "test");
	printf("########################################### decodebin\n");
	return true;
}

gint * RtspTask::autoplug_select_callback(GstElement * bin, GstPad * pad, GstCaps * caps, GstElementFactory * factory1, gpointer udata)
{
	RtspTask *pTask = (RtspTask *)udata;
	//GstUtil::print_pad_templates_information(factory1);
	string decname = gst_element_factory_get_longname(factory1);
	puts("======================+++++++++++++++++++++++++++++++++++++=============================");
	puts(decname.c_str());
	if (decname != "NVIDIA v4l2 video decoder")
	{
		puts("selected-------------------------------------------0");
		return &GST_AUTOPLUG_SELECT_TRY;
	}
	else {
		puts("selected-------------------------------------------1");
		return &GST_AUTOPLUG_SELECT_SKIP;
	}

}
int cpNV12(unsigned char* dst, unsigned char* src, int width, int height, int padlen) {

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

int cpNV12_drop(unsigned char* dst, unsigned char* src, int width, int height, int padlen, int idrop) {

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

GstFlowReturn RtspTask::on_video_sample(GstElement* sink, gpointer ptr) {

	RtspTask * pTask = (RtspTask*)ptr;
	GstSample* videoSample;

	if (time(NULL) - pTask->pFlow_->task_start_time_ > 10) {
		pTask->myfps(0);
	}

	//	puts("----- on video =====");
	g_signal_emit_by_name(sink, "pull-sample", &videoSample);

	while (videoSample) {
		GstBuffer* buf = gst_sample_get_buffer(videoSample);
		GstVideoMeta *meta = gst_buffer_get_video_meta(buf);
		guint nplanes = GST_VIDEO_INFO_N_PLANES(&(pTask->videoinfo_));
		GstVideoFormat pixfmt = GST_VIDEO_INFO_FORMAT(&(pTask->videoinfo_));
		const char *pixfmt_str = gst_video_format_to_string(pixfmt);

	//		printf("1111----------video pts:%ld ,  dts=%ld----------\n", GST_BUFFER_PTS(buf), GST_BUFFER_DTS(buf));
		if (pTask->lastVideoStmp_ == 0) {
			pTask->lastVideoStmp_ = GST_BUFFER_PTS(buf);
		}


		GstMapInfo map_info;
		if (!gst_buffer_map(buf, &map_info, (GstMapFlags)GST_MAP_READ))
		{
			g_print("gst_buffer_map() error!---eos \n");
			break;
		}

		

		//pTask->last_pts_ = GST_BUFFER_PTS(buf);

		if (pTask->framecount_ == 0 && pTask->video_start_ms_==0) {
			pTask->m_source_width = GST_VIDEO_INFO_WIDTH(&(pTask->videoinfo_));
			pTask->m_source_height = GST_VIDEO_INFO_HEIGHT(&(pTask->videoinfo_));
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



				if (pTask->pFlow_->src_fs_d_ != 0 && pTask->pFlow_->src_fs_n_!=0) {
					pTask->tmStamp_ms = 1000 / (pTask->pFlow_->src_fs_n_ / pTask->pFlow_->src_fs_d_);
					puts("))))))))))))))");
				}
				//printf("------------ fps = %d %d %d-----\n", pTask->pFlow_->src_fs_d_, pTask->pFlow_->src_fs_n_, pTask->tmStamp_ms);
				if (pTask->pFlow_->src_fs_n_ == 0 || pTask->pFlow_->src_fs_n_ / pTask->pFlow_->src_fs_d_ > 30) {
					//pTask->need_measure_fps_ = 1;
					//pTask->video_start_ms_ = ts_time::currentpts(pTask->pFlow_->task_start_time_);

					pTask->pFlow_->src_fs_n_ = 25;
					pTask->pFlow_->src_fs_d_ = 1;
				}

			}
				
		//	pTask->pFlow_->task_start_time_ = ts_time::current();
			pTask->pFlow_->pixel_fmt_ = pixfmt_str;
			DebugLog::writeLogF("===================================\n");
			DebugLog::writeLogF("Rtsp Task video stream information:\n");
			DebugLog::writeLogF("  taskid: %s\n", pTask->pFlow_->taskid_.c_str());
			DebugLog::writeLogF("  rtspurl: %s\n", pTask->pFlow_->rtspUrl_.c_str());
			DebugLog::writeLogF("  video start time: %d\n", ts_time::current());
			DebugLog::writeLogF("  size: %u x %u pixel\n", pTask->m_source_width, pTask->m_source_height);
			DebugLog::writeLogF("  pixel format: %s  number of planes: %u\n", pixfmt_str, nplanes);
			//	printf("  video meta found: %d\n", (meta != NULL));
			DebugLog::writeLogF("  mpp frame size : %d \n", map_info.size);
			DebugLog::writeLogF("===================================\n\n");

			if (ts_time::current() - pTask->pFlow_->task_start_time_ > 6) {
				printf("++++++++task starttime = %d\n", pTask->pFlow_->task_start_time_);
				if (pTask->needteardown_ != 1) {
					pTask->closestyle_ = 6;
					//gst_bus_post(pTask->bus_, gst_message_new_eos(GST_OBJECT_CAST(pTask->rtspsrc_)));
					pTask->reportStatus();
					pTask->needteardown_ = 1;
				}

			}
		}

		int source_width = pTask->m_source_width;
		int source_height = pTask->m_source_height;
		//printf("1111----------video size:old:%d ,  %d  new:%d ,  %d----------\n", source_width, source_height, GST_VIDEO_INFO_WIDTH(&(pTask->videoinfo_)), GST_VIDEO_INFO_HEIGHT(&(pTask->videoinfo_)));

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

		//puts("------------------------one frame----------------------------");
		if (pTask->pFlow_->pRawData_in->size() < pTask->pFlow_->buf_decode_) {

			//		puts("==================================one frame==============");
				/*	FILE* yuvout = fopen("tstYuv0602.yuv", "w");
					fwrite(map_info.data, map_info.size, 1, yuvout);
					fclose(yuvout);
					exit(0);
					*/
			
			Packet* newData;

			if (TaskManager::GetInstance()->chip_type_ == CHIP_SOFTWARE) {
				newData = new Packet(map_info.data, map_info.size);
				newData->size = map_info.size;
				newData->type_ = 1;//video
				newData->width = source_width;
				newData->height = source_height;
			}
			else
				if (TaskManager::GetInstance()->chip_type_ == CHIP_ROCKCHIP || TaskManager::GetInstance()->chip_type_ == CHIP_NVV || TaskManager::GetInstance()->chip_type_ == CHIP_TRYCPU) {
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
								int yuvsize = cpNV12(pTmp, map_info.data, source_width, source_height, padlen);
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
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 640, 368, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);*/

							newData = new Packet(map_info.data, map_info.size);
							newData->size = map_info.size;
							newData->type_ = 1;//video

							newData->width = 640;
							newData->height = 368;

						}
						else if (map_info.size == 1044480) { // 1200*536
							int padlen = 80;
							unsigned char* pTmp = (unsigned char*)malloc(1280 * 544 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 1200, 544, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 1200;
							newData->height = 536;

						}
						else if (map_info.size == 8107008) { // 2880*1620
							int padlen = 448;
							unsigned char* pTmp = (unsigned char*)malloc(2880 * 1632 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 2880, 1624, padlen, 4);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 2880;
							newData->height = 1620;

						}
						else if (map_info.size == 7050240) { // 2880*1620
							int padlen = 0;
							unsigned char* pTmp = (unsigned char*)malloc(2880 * 1632 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 2880, 1632, padlen, 12);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 2880;
							newData->height = 1620;

						}
						else if (map_info.size == 979200) { // 1200*536
							int padlen = 0;
							unsigned char* pTmp = (unsigned char*)malloc(1200 * 544 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 1200, 544, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 1200;
							newData->height = 536;

						}
						else if (map_info.size == 6868992) { // 3328*1376 (3040*1368)
							int padlen = 288;
							unsigned char* pTmp = (unsigned char*)malloc(3328 * 1376 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 3040, 1376, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 3040;
							newData->height = 1368;

						}
						else if (map_info.size == 4534272) { // 2304*1296(+16)
							int padlen = 0;
							unsigned char* pTmp = (unsigned char*)malloc(2304 * 1312 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 2304, 1312, padlen, 16);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 2304;
							newData->height = 1296;

						}
						else if (map_info.size == 6193152) { // 2688*1520(+16)
							int padlen = 0;
							unsigned char* pTmp = (unsigned char*)malloc(2688 * 1536 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 2688, 1536, padlen, 16);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 2688;
							newData->height = 1520;

						}
						else if (map_info.size == 6488064) { // 2688(+128)*1520(+16)
							int padlen = 128;
							unsigned char* pTmp = (unsigned char*)malloc(2816 * 1536 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 2688, 1536, padlen, 16);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 2688;
							newData->height = 1520;

						}
						else if (map_info.size == 7589376) { // 2592*1944(+8)
							int padlen = 0;
							unsigned char* pTmp = (unsigned char*)malloc(2592 * 1952 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 2592, 1952, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 2592;
							newData->height = 1944;

						}
						else if (map_info.size == 6274560) { // 3040*1376 (3040*1368)
							int padlen = 0;
							unsigned char* pTmp = (unsigned char*)malloc(3040 * 1376 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 3040, 1376, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 3040;
							newData->height = 1368;

						}
						else if (map_info.size == 423936) { // 640*360
							int padlen = 128;
							unsigned char* pTmp = (unsigned char*)malloc(640 * 368 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 640, 368, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 640;
							newData->height = 360;

						}
						else if (map_info.size == 3760128) {
							int padlen = 384;
							unsigned char* pTmp = (unsigned char*)malloc(1920 * 1088 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 1920, 1088, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 1920;
							newData->height = 1080;

						}
						else if (map_info.size == 6266880) {
							int padlen = 0;
							unsigned char* pTmp = (unsigned char*)malloc(3840 * 1088 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 3840, 1088, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 3840;
							newData->height = 1080;

						}
						else if (map_info.size == 1566720) {
							int padlen = 0;
							unsigned char* pTmp = (unsigned char*)malloc(1920 * 544 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 1920, 544, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 1920;
							newData->height = 536;

						}
						else if (map_info.size == 1880064) {
							int padlen = 384;
							unsigned char* pTmp = (unsigned char*)malloc(2304 * 544 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 1920, 544, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 1920;
							newData->height = 536;

						}
						else if (map_info.size == 3133440 || map_info.size == 4177920) {// h264 16bit 1920 * 1080 == 1920 * 1088

							int padlen = 0;
							unsigned char* pTmp = (unsigned char*)malloc(1920 * 1080 * 3 / 2);
							int yuvsize = cpNV12_drop(pTmp, map_info.data, 1920, 1088, padlen, 8);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 1920;
							newData->height = 1080;

						}
						else if (map_info.size == 8146944) {// 2880 * 1620 2880 * 1632
							int padlen = 448;
							unsigned char* pTmp = (unsigned char*)malloc(2880 * 1620 * 3 / 2);
							int yuvsize = cpNV12(pTmp, map_info.data, 2880, 1620, padlen);
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
							int yuvsize = cpNV12(pTmp, map_info.data, 1920, 1080, padlen);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);
							newData->width = 1920;
							newData->height = 1080;
						}
						else if (map_info.size == 6082560 || map_info.size == 8110080) {// h265 2560 * 1440 256 2816 1584
							int padlen = 256;
							unsigned char* pTmp = (unsigned char*)malloc(2560 * 1440 * 3 / 2);
							int yuvsize = cpNV12(pTmp, map_info.data, 2560, 1440, padlen);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);
							newData->width = 2560;
							newData->height = 1440;
						}
						else if (map_info.size == 6420480) {//  2688 1520
							int padlen = 128;
							unsigned char* pTmp = (unsigned char*)malloc(2688 * 1520 * 3 / 2);
							int yuvsize = cpNV12(pTmp, map_info.data, 2688, 1520, padlen);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);
							newData->width = 2688;
							newData->height = 1520;
						}

						else if (map_info.size == 8626176) {// 3072 * 1728 
							int padlen = 256;
							unsigned char* pTmp = (unsigned char*)malloc(3072 * 1728 * 3 / 2);
							int yuvsize = cpNV12(pTmp, map_info.data, 3072, 1728, padlen);
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

				if (pTask->tmStamp_ms != 0) {
					pTask->lastVideoStmp_ += pTask->tmStamp_ms * 1000000;
					newData->pts_ = pTask->lastVideoStmp_;

					//printf("------------video tmstamp = %ld %ld\n", pTask->tmStamp_ms, newData->pts_);
				}
				else
				{
					newData->pts_ = GST_BUFFER_PTS(buf);
				}

				pTask->last_pts_ = newData->pts_;
				
			newData->seq_ = pTask->cur_seq_++;

			if (pTask->need_measure_fps_ == 1) {
				//pTask->pFlow_->pRawData_in->cache(newData);
				static int cachesize = 0;
				cachesize++;
				delete newData;

				unsigned long tmpms = ts_time::currentpts(pTask->pFlow_->task_start_time_);

				printf("------------cache frame------%d  %d\n", tmpms, pTask->video_start_ms_);
				if (tmpms - pTask->video_start_ms_ > 950) {
					//cachesize = 0;
					if (cachesize >= 30) cachesize = 25;

					pTask->pFlow_->src_fs_d_ = 1;
					pTask->pFlow_->src_fs_n_ = cachesize;// pTask->pFlow_->pRawData_in->size();
					printf(">>>>>>>>>>>>>>>>>>>>>>>>> flush frame:%d\n", cachesize);//pTask->pFlow_->pRawData_in->size());
					pTask->need_measure_fps_ = 0;
					//pTask->pFlow_->pRawData_in->flush();
				}
			}
			else {
				pTask->pFlow_->pRawData_in->push(newData);
				pTask->framecount_++;
//				printf("++++++++++++++ video pts:%ld\n", newData->pts_);
			}
			
				//printf("			++++++++++++++++++++++  new video ts :%ld  seq=%d\n", newData->pts_, newData->seq_);
				/*if (pTask->cur_seq_ > 3) {
					sleep(1);

					exit(0);
				}*/
		}
		else {
		printf("=========  drop video frame ============\n");
		}
		gst_buffer_unmap(buf, &map_info);
		gst_sample_unref(videoSample);



		return GST_FLOW_OK;
	}

	return GST_FLOW_ERROR;
}

#define AUDIO_SAMP_BLK	640
GstFlowReturn RtspTask::on_audio_sample(GstElement* sink, gpointer ptr) {

	static int is_audio_got = 0;
	RtspTask * pTask = (RtspTask*)ptr;
	GstSample* audioSample;


	g_signal_emit_by_name(sink, "pull-sample", &audioSample);

	if (audioSample)
	{
		if (is_audio_got == 0) {
			is_audio_got = 1;
			printf("---------audio start time:%d\n", ts_time::current());
		}
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
		if ((pTask->framecount_ > 0 || pTask->tmStamp_ms !=0)) {

			if (pTask->lastAudioStmp_ == 0) {
				if (pTask->last_pts_ != 0) {
					pTask->lastAudioStmp_ = pTask->last_pts_;
				}
				else {
				pTask->lastAudioStmp_ = GST_BUFFER_PTS(buf);
				}
			}
			else if (llabs(pTask->lastAudioStmp_ - pTask->last_pts_) > 40000000){
				if (pTask->last_pts_ != 0) {
					pTask->lastAudioStmp_ = pTask->last_pts_;
				//	printf("========================  update pts again =========================\n");
				}
			}

			if (pTask->pFlow_->pAudioData_in->size() < 50)
			{
				if (pTask->shareAudioLen + map_info.size >= 20480) {
					memcpy(pTask->shareAudiobuf_ + pTask->shareAudioLen, map_info.data, 20480- pTask->shareAudioLen);
					pTask->shareAudioLen =20480;
				}
				else {
					memcpy(pTask->shareAudiobuf_ + pTask->shareAudioLen, map_info.data, map_info.size);
					pTask->shareAudioLen += map_info.size;
				}
			

				int blksize = pTask->shareAudioLen / AUDIO_SAMP_BLK;

				for (int i = 0; i < blksize; i++) {
					Packet* newData = new Packet(pTask->shareAudiobuf_ + i * AUDIO_SAMP_BLK, AUDIO_SAMP_BLK);
					newData->type_ = 2;//audio
					newData->size = AUDIO_SAMP_BLK;
					pTask->lastAudioStmp_ = pTask->lastAudioStmp_ + 40 * 1000000;
					newData->pts_ =  pTask->lastAudioStmp_;
					pTask->pFlow_->pAudioData_in->push(newData);

					// printf("------audio pts info   %ld %ld len=%d\n", GST_BUFFER_PTS(buf), newData->pts_, map_info.size);

					pTask->shareAudioLen -= AUDIO_SAMP_BLK;
				}

				if (pTask->shareAudioLen > 0) {
					memmove(pTask->shareAudiobuf_, pTask->shareAudiobuf_ + blksize * AUDIO_SAMP_BLK, pTask->shareAudioLen);
				}
			}
			else
			{
				if (pTask->last_pts_ != 0) {
					pTask->lastAudioStmp_ = pTask->last_pts_;
				}
				else {
				pTask->lastAudioStmp_ = GST_BUFFER_PTS(buf);
				}
				pTask->pFlow_->pAudioData_in->clear();
				puts("-------------- audio buff over load -------------------");
			}
		}
		else
		{
			puts("==============audio drop================");
			if (ts_time::current() - pTask->pFlow_->task_start_time_ > 6) {
			//	printf("task starttime = %d\n", pTask->pFlow_->task_start_time_);
				if (pTask->needteardown_ != 1) {
					pTask->closestyle_ = 6;
					//gst_bus_post(pTask->bus_, gst_message_new_eos(GST_OBJECT_CAST(pTask->rtspsrc_)));
					pTask->reportStatus();
					pTask->needteardown_ = 1;
				}

			}

		}

		//puts("=== on audio ===");


		//	printf("audio frame info----------%d\n", map_info.size);
		//printf("------audio pts info   %ld len=%d\n", GST_BUFFER_PTS(buf), map_info.size);

		gst_buffer_unmap(buf, &map_info);
		gst_sample_unref(audioSample);
	}
	return GST_FLOW_OK;


}

