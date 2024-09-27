#include "FilesrcTask.h"
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

FilesrcTask::FilesrcTask(TaskFlow* pFlow)
{
	pFlow_ = pFlow;
	//strUrl_ = pFlow->rtspUrl_; //
	withaudio_ = pFlow->withaudio_;
	tid_ = TaskManager::total_tid++;
	gst_init(NULL, NULL);
	puts("create FilesrcTask");
}


FilesrcTask::~FilesrcTask() {
	puts("~delete FilesrcTask");
	//releasePileline();
	reportStatus();
	if (pFlow_->encodingStyle_ != "file") {
		Packet* newData = new Packet();
		pFlow_->pRawData_in->push(newData);
		Packet* newData1 = new Packet();
		pFlow_->pAudioData_in->push(newData1);
		pFlow_->onComplete(2);
	}
}

void FilesrcTask::reportStatus()
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
	else if (closestyle_ == 5) {
		root["code"] = ERR_DECODE_ERR;
		root["message"] = "build pipeline fail, check gstreamer plugin on the device";

	}

	TaskManager::GetInstance()->onAnyWorkStatus(root, 0);

}
void *FilesrcTask::FilesrcTask::startThread(TaskFlow* pFlow)
{
	FilesrcTask* thread = new FilesrcTask(pFlow);

	thread->setAutoDelete(true);
	thread->start();

	return thread;
}
void FilesrcTask::stop()
{

	closestyle_ = 1;
	needteardown_ = 1;
	bool ret = gst_bus_post(bus_, gst_message_new_eos(GST_OBJECT_CAST(filesrc_)));
	//printf("stop rtsp task--------------------%d %s \n", ret, pFlow_->taskid_.c_str());

}
bool FilesrcTask::FilesrcTask::onStart()
{
	return true;
}
void FilesrcTask::FilesrcTask::run()
{

	isRun = STATUS_CONNECTING;


	int ret = 0;
	if (pFlow_->encodingStyle_ == "file") {
		if (pFlow_->inputtype_ == "raw") {
			puts("-----------------raw file src-------------------------");
			ret = buildRawPipeline();
		}
		else {
			ret = buildFastPipeline();
		}
	}
	else if (pFlow_->inputtype_ == "mp4") {
		puts("-----------------mp4 file src-------------------------");
		ret = buildMp4Pipeline();
	}
	else if (pFlow_->inputtype_ == "raw") {
		puts("-----------------raw file src-------------------------");
		ret = buildRawPipeline1();
	}
	else if (pFlow_->inputtype_ == "ts") {
		puts("-----------------ts file src-------------------------");

		ret = buildTsPipeline();

	}

	if (ret < 0)
	{
		isRun = STATUS_DISCONNECT;
		DebugLog::writeLogF("file src build pipe line failed :%s\n", pFlow_->rtspUrl_.c_str());
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

static gboolean before_send_callback(GstElement * rtspsrc, GstRTSPMessage * msg, gpointer udata)
{
	FilesrcTask *pTask = (FilesrcTask *)udata;
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

int FilesrcTask::selectVideoEncoder()
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
			g_object_set(G_OBJECT(videoencoder_), "bps", pFlow_->bitrate_ * 1024 * 10, NULL);
			g_object_set(G_OBJECT(videoencoder_), "bps-max", pFlow_->bitrate_ * 1024 * 10, NULL);
			g_object_set(G_OBJECT(videoencoder_), "gop", pFlow_->gop_, NULL);
			//g_object_set(G_OBJECT(videoencoder_), "profile", pFlow_->profile_, NULL);
			g_object_set(G_OBJECT(videoencoder_), "rc-mode", pFlow_->rcmode_, NULL);

			guint curbps = 0;
			g_object_get(G_OBJECT(videoencoder_), "bps", &curbps, NULL);
			guint mxbpx = 0;
			g_object_get(G_OBJECT(videoencoder_), "bps-max", &mxbpx, NULL);


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
			//	printf("set %s: type:%d val:", (char*)paramname, pFlow_->x264_extra_[*iter].type());
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
				//		printf(" val %d \n", pFlow_->x264_extra_[*iter].asUInt());
				g_object_set(G_OBJECT(videoencoder_), paramname, iVal, NULL);
			}
			else if (pFlow_->x264_extra_[*iter].type() == TSJson::intValue)
			{
				int iVal = pFlow_->x264_extra_[*iter].asInt();
				//		printf(" val %d \n", pFlow_->x264_extra_[*iter].asInt());
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


int FilesrcTask::buildRawPipeline1() {
	puts("----------------build ts pipeline-------------");
	// Build Pipeline 
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	filesrc_ = gst_element_factory_make("filesrc", ("filesrc0" + std::to_string(tid_)).c_str());
	//filedemuxer_ = gst_element_factory_make("tsdemux", ("filedemux" + std::to_string(tid_)).c_str());
	videodecoder_ = gst_element_factory_make("decodebin", ("videodecodebin" + std::to_string(tid_)).c_str());
	videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	appvideosink_ = gst_element_factory_make("appsink", ("videoapp_sink" + std::to_string(tid_)).c_str());

	cpasfilter_ = gst_element_factory_make("capsfilter", NULL);
	if (!cpasfilter_) {
		puts("--- caps filter created failed ---");
	}

	gst_util_set_object_arg(G_OBJECT(cpasfilter_), "caps",
		"video/x-raw, "
		"format={  NV12 }");

	if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV)
	{
		vconverter_ = gst_element_factory_make("nvvideoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	}
	else {
		vconverter_ = gst_element_factory_make("videoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	}


	if (!pipeline_ | !videodecoder_ || !cpasfilter_ || !videoqueue_ || !appvideosink_) {
		g_printerr("One element could not be created 2.\n");
		return -1;
	}

	// appsink setting
	g_object_set(G_OBJECT(appvideosink_), "sync", true, NULL);
	GstPad* apppad = gst_element_get_static_pad(appvideosink_, "sink");
	gst_pad_add_probe(apppad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, appsink_query_cb, NULL, NULL);
	gst_object_unref(apppad);

	gst_base_sink_set_max_lateness(GST_BASE_SINK(appvideosink_), 70 * GST_MSECOND);
	gst_base_sink_set_qos_enabled(GST_BASE_SINK(appvideosink_), TRUE);
	g_object_set(G_OBJECT(appvideosink_), "max-buffers", this->pFlow_->buf_input_/2, NULL);
	gst_pad_add_probe(gst_element_get_static_pad(appvideosink_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe, this, NULL);
	g_object_set(G_OBJECT(appvideosink_), "emit-signals", TRUE, NULL);
	g_signal_connect(G_OBJECT(appvideosink_), "new-sample", G_CALLBACK(FilesrcTask::on_video_sample), this);
	gst_app_sink_set_max_buffers(GST_APP_SINK(appvideosink_), 1);
	gst_app_sink_set_buffer_list_support(GST_APP_SINK(appvideosink_), FALSE);


	//rtsp setting;
	g_object_set(G_OBJECT(filesrc_), "location", pFlow_->rtspUrl_.c_str(), NULL);


	//Partially static pipeline link
	gst_bin_add_many(GST_BIN(pipeline_), filesrc_,  videodecoder_, vconverter_, cpasfilter_, appvideosink_, NULL);

	if (!gst_element_link(filesrc_, videodecoder_)) {
		g_printerr("filesrc 1 pipeline Elements could not be linked.\n");

	}

	if (!gst_element_link_many(vconverter_, cpasfilter_, appvideosink_, NULL)) {
		g_printerr("decoder pipeline Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	//Dynamic link
	g_signal_connect(videodecoder_, "pad-added", G_CALLBACK(on_dec2conv_link), this);

	return 0;
}

int FilesrcTask::buildRawPipeline() {
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	filesrc_ = gst_element_factory_make("filesrc", ("filesrc0" + std::to_string(tid_)).c_str());
	//filedemuxer_ = gst_element_factory_make("h264parse", ("filedemux" + std::to_string(tid_)).c_str());
	//videodecoder_ = gst_element_factory_make("mppvideodec", ("videodecodebin" + std::to_string(tid_)).c_str());

	videodecoder_ = gst_element_factory_make("decodebin", ("videodecodebin" + std::to_string(tid_)).c_str());

	//videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	appvideosink_ = gst_element_factory_make("filesink", ("videoapp_sink" + std::to_string(tid_)).c_str());

	//vconverter_ = gst_element_factory_make("videoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	
	selectVideoEncoder();

	if (pFlow_->encodetype_ == "h264") {
		videoparser_ = gst_element_factory_make("h264parse", ("parser" + std::to_string(tid_)).c_str());
	}
	else if (pFlow_->encodetype_ == "h265") {
		videoparser_ = gst_element_factory_make("h265parse", ("parser" + std::to_string(tid_)).c_str());
	}

	if (pFlow_->outputtype_ == "mp4") {
		avmuxer_ = gst_element_factory_make("qtmux", ("muxer" + std::to_string(tid_)).c_str());
	}
	else if (pFlow_->outputtype_ == "flv") {
		avmuxer_ = gst_element_factory_make("flvmux", ("muxer" + std::to_string(tid_)).c_str());
	}
	else if (pFlow_->outputtype_ == "ts") {
		avmuxer_ = gst_element_factory_make("mpegtsmux", ("muxer" + std::to_string(tid_)).c_str());
	}
	else {
		g_printerr("output type not surpport.\n");
		return -1;

	}

	if (!pipeline_ || !filesrc_ || !videodecoder_ || !vconverter_ || !videoencoder_ || !videoparser_ || !avmuxer_ || !appvideosink_ ) {
		g_printerr("One element could not be created 3 in raw.\n");
		return -1;
	}



	g_object_set(appvideosink_, "location", pFlow_->outputpath_.c_str(), NULL);
	g_object_set(appvideosink_, "o-sync", false, NULL);
	//g_object_set(appvideosink_, "throttle-time ", 1000*1000*1000, NULL);
	g_object_set(G_OBJECT(filesrc_), "location", pFlow_->rtspUrl_.c_str(), NULL);


	//Partially static pipeline link
	gst_bin_add_many(GST_BIN(pipeline_), filesrc_,  videodecoder_, vconverter_, videoencoder_, videoparser_, avmuxer_, appvideosink_, NULL);

	if (!gst_element_link_many(filesrc_,  videodecoder_, NULL)) {
		g_printerr("raw data pipeline Elements could not be linked in raw 0.\n");
		gst_object_unref(pipeline_);
		return -1;
	}
	if (!gst_element_link_many( vconverter_, videoencoder_, videoparser_, avmuxer_, appvideosink_, NULL)) {
		g_printerr("raw data pipeline Elements could not be linked in raw 1.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	g_signal_connect(videodecoder_, "pad-added", G_CALLBACK(on_dec2conv_link), this);

	return 0;
}
int FilesrcTask::buildFastPipeline() {
	// Build Pipeline 
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	filesrc_ = gst_element_factory_make("filesrc", ("filesrc0" + std::to_string(tid_)).c_str());
	filedemuxer_ = gst_element_factory_make("qtdemux", ("filedemux" + std::to_string(tid_)).c_str());
	videodecoder_ = gst_element_factory_make("decodebin", ("videodecodebin" + std::to_string(tid_)).c_str());
	//videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	appvideosink_ = gst_element_factory_make("filesink", ("videoapp_sink" + std::to_string(tid_)).c_str());



	vconverter_ = gst_element_factory_make("videoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	videoencoder_ = gst_element_factory_make("mpph264enc", ("encoder" + std::to_string(tid_)).c_str());
	videoparser_ = gst_element_factory_make("h264parse", ("parser" + std::to_string(tid_)).c_str());
	avmuxer_ = gst_element_factory_make("mpegtsmux", ("muxer" + std::to_string(tid_)).c_str());


	g_object_set(G_OBJECT(videoencoder_), "header-mode", 1, NULL);
	g_object_set(G_OBJECT(videoencoder_), "max-reenc", 0, NULL);
	g_object_set(G_OBJECT(videoencoder_), "min-force-key-unit-interval", 0, NULL);
	g_object_set(G_OBJECT(videoencoder_), "sei-mode", 1, NULL);


	//	printf("===========mpp 264========set bps================= %d\n", pFlow_->bitrate_ * 1024);
	g_object_set(G_OBJECT(videoencoder_), "bps", pFlow_->bitrate_ * 1024, NULL);
	g_object_set(G_OBJECT(videoencoder_), "bps-max", pFlow_->bitrate_ * 1024, NULL);
	g_object_set(G_OBJECT(videoencoder_), "gop", pFlow_->gop_, NULL);
	//g_object_set(G_OBJECT(videoencoder_), "profile", pFlow_->profile_, NULL);
	g_object_set(G_OBJECT(videoencoder_), "rc-mode", pFlow_->rcmode_, NULL);

	guint curbps = 0;
	g_object_get(G_OBJECT(videoencoder_), "bps", &curbps, NULL);
	guint mxbpx = 0;
	g_object_get(G_OBJECT(videoencoder_), "bps-max", &mxbpx, NULL);




	if (!pipeline_ | !videodecoder_  || !appvideosink_) {
		g_printerr("One element could not be created 3.\n");
		return -1;
	}

	// appsink setting
/*	g_object_set(G_OBJECT(appvideosink_), "sync", false, NULL);
	GstPad * apppad = gst_element_get_static_pad(appvideosink_, "sink");
	gst_pad_add_probe(apppad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, appsink_query_cb, NULL, NULL);
	gst_object_unref(apppad);

	gst_base_sink_set_max_lateness(GST_BASE_SINK(appvideosink_), 70 * GST_MSECOND);
	gst_base_sink_set_qos_enabled(GST_BASE_SINK(appvideosink_), TRUE);
	g_object_set(G_OBJECT(appvideosink_), "max-buffers", 10, NULL);
	gst_pad_add_probe(gst_element_get_static_pad(appvideosink_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe, this, NULL);
	g_object_set(G_OBJECT(appvideosink_), "emit-signals", TRUE, NULL);
	g_signal_connect(G_OBJECT(appvideosink_), "new-sample", G_CALLBACK(FilesrcTask::on_video_sample), this);
	gst_app_sink_set_max_buffers(GST_APP_SINK(appvideosink_), 1);
	gst_app_sink_set_buffer_list_support(GST_APP_SINK(appvideosink_), FALSE);
	*/

	char dstFilename[256] = { 0 };
	//sprintf(dstFilename, "%s/%s_%s_%d.%s", pFlow_->outputpath_.c_str(), pFlow_->taskid_.c_str(), pFlow_->encodetype_.c_str(), time(NULL), FileSuffix.c_str());
	sprintf(dstFilename, "%s", pFlow_->outputpath_.c_str());
	puts(dstFilename);
	g_object_set(appvideosink_, "location", dstFilename, NULL);
	g_object_set(appvideosink_, "o-sync", false, NULL);
	//g_object_set(appvideosink_, "throttle-time ", 1000*1000*1000, NULL);
	

	g_object_set(G_OBJECT(filesrc_), "location", pFlow_->rtspUrl_.c_str(), NULL);


	//Partially static pipeline link
	gst_bin_add_many(GST_BIN(pipeline_), filesrc_, filedemuxer_, videodecoder_, vconverter_, videoencoder_, videoparser_, avmuxer_, appvideosink_, NULL);

	if (!gst_element_link(filesrc_, filedemuxer_)) {
		g_printerr("filesrc 1 pipeline Elements could not be linked.\n");

	}

	if (!gst_element_link_many(vconverter_, videoencoder_, videoparser_, avmuxer_, appvideosink_, NULL)) {
		g_printerr("decoder pipeline Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	//Dynamic link
	g_signal_connect(filedemuxer_, "pad-added", G_CALLBACK(on_demuxer2dec_link), this);
	g_signal_connect(videodecoder_, "pad-added", G_CALLBACK(on_dec2conv_link), this);

	return 0;
}

static void
on_src_tee_added(GstElement* element, GstPad* pad, gpointer data) {
	// GstPad *sinkpad;
	FilesrcTask* pTask = (FilesrcTask*)data;

	/* We can now link this pad with the rtsp-decoder sink pad */
	g_print("Dynamic pad created, linking source/demuxer\n");
	GstPad* tee_sinkpad = gst_element_get_static_pad(pTask->tee_, "sink");
	gst_pad_link(pad, tee_sinkpad);
	gst_object_unref(tee_sinkpad);
}

int FilesrcTask::buildMp4Pipeline() {

	// Build Pipeline 
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	filesrc_ = gst_element_factory_make("filesrc", ("filesrc0" + std::to_string(tid_)).c_str());
	//tee_ = gst_element_factory_make("tee", ("tee" + std::to_string(tid_)).c_str());
	filedemuxer_ = gst_element_factory_make("qtdemux", ("filedemux" + std::to_string(tid_)).c_str());
	videodecoder_ = gst_element_factory_make("decodebin", ("videodecodebin" + std::to_string(tid_)).c_str());
	videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	appvideosink_ = gst_element_factory_make("appsink", ("videoapp_sink" + std::to_string(tid_)).c_str());

	cpasfilter_ = gst_element_factory_make("capsfilter", NULL);
	if (!cpasfilter_) {
		puts("--- caps filter created failed ---");
	}

	gst_util_set_object_arg(G_OBJECT(cpasfilter_), "caps",
		"video/x-raw, "
		"format={  NV12 }");

	if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV)
	{
		vconverter_ = gst_element_factory_make("nvvideoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	}
	else {
		vconverter_ = gst_element_factory_make("videoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	}


	if (!pipeline_ | !videodecoder_ || !cpasfilter_ || !videoqueue_ || !appvideosink_) {
		g_printerr("One element could not be created 2.\n");
		return -1;
	}

	// appsink setting
	g_object_set(G_OBJECT(appvideosink_), "sync", true, NULL);
	/*GstPad * apppad = gst_element_get_static_pad(appvideosink_, "sink");
	gst_pad_add_probe(apppad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, appsink_query_cb, NULL, NULL);
	gst_object_unref(apppad);*/

	gst_base_sink_set_max_lateness(GST_BASE_SINK(appvideosink_), 70 * GST_MSECOND);
	gst_base_sink_set_qos_enabled(GST_BASE_SINK(appvideosink_), TRUE);
	g_object_set(G_OBJECT(appvideosink_), "max-buffers", this->pFlow_->buf_input_/3, NULL);
	gst_pad_add_probe(gst_element_get_static_pad(appvideosink_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe, this, NULL);
	g_object_set(G_OBJECT(appvideosink_), "emit-signals", TRUE, NULL);
	g_signal_connect(G_OBJECT(appvideosink_), "new-sample", G_CALLBACK(FilesrcTask::on_video_sample), this);
	gst_app_sink_set_max_buffers(GST_APP_SINK(appvideosink_), 1);
	gst_app_sink_set_buffer_list_support(GST_APP_SINK(appvideosink_), FALSE);


	g_object_set(G_OBJECT(filesrc_), "location", pFlow_->rtspUrl_.c_str(), NULL);


	//Partially static pipeline link
	gst_bin_add_many(GST_BIN(pipeline_), filesrc_, filedemuxer_, videodecoder_, vconverter_, cpasfilter_, appvideosink_, NULL);
	
	if (!gst_element_link_many(filesrc_, filedemuxer_, NULL)) {
		g_printerr("filesrc 1 pipeline Elements could not be linked.\n");

	}

	/*GstPad* queue2_video_pad = gst_element_get_static_pad(filedemuxer_, "sink");
	GstPad* tee2_video_pad = gst_element_get_request_pad(tee_, "src_%u");
	if (gst_pad_link(tee2_video_pad, queue2_video_pad) != GST_PAD_LINK_OK) {
		g_printerr("tee link queue error. \n");
		gst_object_unref(pipeline_);
		return -1;
	}
	gst_object_unref(queue2_video_pad);
	gst_object_unref(tee2_video_pad);*/

	if (!gst_element_link_many(vconverter_, cpasfilter_, appvideosink_, NULL)) {
		g_printerr("decoder pipeline Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	

	//addAudioBranch();
	//g_signal_connect(filesrc_, "pad-added", G_CALLBACK(on_src_tee_added), this);
	//Dynamic link
	g_signal_connect(filedemuxer_, "pad-added", G_CALLBACK(on_demuxer2dec_link), this);
	g_signal_connect(videodecoder_, "pad-added", G_CALLBACK(on_dec2conv_link), this);

	return 0;
}

int FilesrcTask::buildTsPipeline() {
	puts("----------------build ts pipeline-------------");
	// Build Pipeline 
	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	filesrc_ = gst_element_factory_make("filesrc", ("filesrc0" + std::to_string(tid_)).c_str());
	filedemuxer_ = gst_element_factory_make("tsdemux", ("filedemux" + std::to_string(tid_)).c_str());
	videodecoder_ = gst_element_factory_make("decodebin", ("videodecodebin" + std::to_string(tid_)).c_str());
	videoqueue_ = gst_element_factory_make("queue", ("queue_appsink" + std::to_string(tid_)).c_str());
	appvideosink_ = gst_element_factory_make("appsink", ("videoapp_sink" + std::to_string(tid_)).c_str());

	cpasfilter_ = gst_element_factory_make("capsfilter", NULL);
	if (!cpasfilter_) {
		puts("--- caps filter created failed ---");
	}

	gst_util_set_object_arg(G_OBJECT(cpasfilter_), "caps",
		"video/x-raw, "
		"format={  NV12 }");

	if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV)
	{
		vconverter_ = gst_element_factory_make("nvvideoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	}
	else {
		vconverter_ = gst_element_factory_make("videoconvert", ("vconvertr" + std::to_string(tid_)).c_str());
	}


	if (!pipeline_ | !videodecoder_ || !cpasfilter_ || !videoqueue_ || !appvideosink_) {
		g_printerr("One element could not be created 2.\n");
		return -1;
	}

	// appsink setting
	g_object_set(G_OBJECT(appvideosink_), "sync", true, NULL);
	GstPad * apppad = gst_element_get_static_pad(appvideosink_, "sink");
	gst_pad_add_probe(apppad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, appsink_query_cb, NULL, NULL);
	gst_object_unref(apppad);

	gst_base_sink_set_max_lateness(GST_BASE_SINK(appvideosink_), 70 * GST_MSECOND);
	gst_base_sink_set_qos_enabled(GST_BASE_SINK(appvideosink_), TRUE);
	g_object_set(G_OBJECT(appvideosink_), "max-buffers", this->pFlow_->buf_input_ / 3, NULL);
	gst_pad_add_probe(gst_element_get_static_pad(appvideosink_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe, this, NULL);
	g_object_set(G_OBJECT(appvideosink_), "emit-signals", TRUE, NULL);
	g_signal_connect(G_OBJECT(appvideosink_), "new-sample", G_CALLBACK(FilesrcTask::on_video_sample), this);
	gst_app_sink_set_max_buffers(GST_APP_SINK(appvideosink_), 1);
	gst_app_sink_set_buffer_list_support(GST_APP_SINK(appvideosink_), FALSE);


	//rtsp setting;
	g_object_set(G_OBJECT(filesrc_), "location", pFlow_->rtspUrl_.c_str(), NULL);


	//Partially static pipeline link
	gst_bin_add_many(GST_BIN(pipeline_), filesrc_, filedemuxer_, videodecoder_, vconverter_, cpasfilter_, appvideosink_, NULL);

	if (!gst_element_link(filesrc_, filedemuxer_)) {
		g_printerr("filesrc 1 pipeline Elements could not be linked.\n");

	}

	if (!gst_element_link_many(vconverter_, cpasfilter_, appvideosink_, NULL)) {
		g_printerr("decoder pipeline Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	//Dynamic link
	g_signal_connect(filedemuxer_, "pad-added", G_CALLBACK(on_demuxer2dec_link), this);
	g_signal_connect(videodecoder_, "pad-added", G_CALLBACK(on_dec2conv_link), this);

	return 0;
}


int FilesrcTask::addAudioBranch()
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
			gst_util_set_object_arg(G_OBJECT(audiocpasfilter_), "caps",
				"audio/x-raw, "
				"format={ S16LE},rate=16000");
		}
		audioresamp_ = gst_element_factory_make("audioresample", NULL);


		//g_object_set(G_OBJECT(appaudiosink_), "sync", true, NULL);
		//gst_base_sink_set_max_lateness(GST_BASE_SINK(appaudiosink_), 70 * GST_MSECOND);
		//gst_base_sink_set_qos_enabled(GST_BASE_SINK(appaudiosink_), TRUE);
		//g_object_set(G_OBJECT(appaudiosink_), "max-buffers", 10, NULL);

		g_object_set(G_OBJECT(appaudiosink_), "sync", true, NULL);
		gst_base_sink_set_max_lateness(GST_BASE_SINK(appaudiosink_), 70 * GST_MSECOND);
		gst_base_sink_set_qos_enabled(GST_BASE_SINK(appaudiosink_), TRUE);
		g_object_set(G_OBJECT(appaudiosink_), "max-buffers", this->pFlow_->buf_input_, NULL);
		g_object_set(G_OBJECT(appaudiosink_), "emit-signals", TRUE, NULL);
		gst_pad_add_probe(gst_element_get_static_pad(appaudiosink_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe1, this, NULL);
		g_signal_connect(G_OBJECT(appaudiosink_), "new-sample", G_CALLBACK(FilesrcTask::on_audio_sample), this);

		
		gst_bin_add_many(GST_BIN(pipeline_), audiodecoder_, aconverter_, audioresamp_, audiocpasfilter_, appaudiosink_, NULL);

		if (!gst_element_sync_state_with_parent(aconverter_)) {
			g_printerr("sync audio element stat faile 0.\n");

		}


		if (!gst_element_sync_state_with_parent(appaudiosink_)) {
			g_printerr("sync audio element stat faile 2.\n");

		}
		if (!gst_element_sync_state_with_parent(audiodecoder_)) {
			g_printerr("sync audio element stat faile 3.\n");

		}
		if (!gst_element_sync_state_with_parent(audiocpasfilter_)) {
			g_printerr("sync audio element stat faile 4.\n");

		}
		if (!gst_element_sync_state_with_parent(audioresamp_)) {
			g_printerr("sync audio element stat faile 4.\n");

		}

		if (!gst_element_link_many(aconverter_, audioresamp_, audiocpasfilter_, appaudiosink_, NULL)) {
			g_printerr("audio branch Elements could not be linked.\n");
			gst_object_unref(pipeline_);
			return -1;
		}
		puts("---------- add audio on_dec2conv_link");
		//g_object_set(G_OBJECT(appaudiosink_), "emit-signals", TRUE, NULL);
		g_signal_connect(audiodecoder_, "pad-added", G_CALLBACK(on_dec2conv_link), this);
		//gst_pad_add_probe(gst_element_get_static_pad(appaudiosink_, "sink"), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, pad_probe1, this, NULL);
		//g_signal_connect(G_OBJECT(appaudiosink_), "new-sample", G_CALLBACK(FilesrcTask::on_audio_sample), this);

		

	}

	return 0;

}

// destroy
void FilesrcTask::releasePileline()
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


GstPadProbeReturn FilesrcTask::pad_probe(GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
	FilesrcTask *pTask = (FilesrcTask *)user_data;
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
GstPadProbeReturn FilesrcTask::pad_probe1(GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
	FilesrcTask *pTask = (FilesrcTask *)user_data;
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


void FilesrcTask::on_demuxer2dec_link(GstElement *element, GstPad *pad, gpointer data) {

	FilesrcTask *pTask = (FilesrcTask *)data;
	g_print("file demuxer received new pad: '%s' from '%s':\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(element));

	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;
	new_pad_caps = gst_pad_get_current_caps(pad);
	new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
	new_pad_type = gst_structure_get_name(new_pad_struct);
	printf("++++++++++++++++++++ pad type: %s\n", new_pad_type);



	if (g_str_has_prefix(new_pad_type, "audio")) {

		if (pTask->withaudio_ == 0 || pTask->is_audio_added == 1)
			return;
		pTask->is_audio_added = 1;
		pTask->addAudioBranch();
	
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
		puts("----------  linked audio pad 1------------\n");

		gst_object_unref(decode_sinkpad);
	}
	else if (g_str_has_prefix(new_pad_type, "video"))
	{
		puts("----------  add video pad------------\n");

		GstPad * tee_sinkpad = gst_element_get_static_pad(pTask->videodecoder_, "sink");
		if (gst_pad_is_linked(tee_sinkpad)) {
			g_print("already linked. Ignoring.\n");
			gst_object_unref(tee_sinkpad);
		}
		pTask->is_video_added = 1;
		gst_pad_link(pad, tee_sinkpad);
		gst_object_unref(tee_sinkpad);
	}

	gst_caps_unref(new_pad_caps);
}



void FilesrcTask::on_dec2conv_link(GstElement *element, GstPad *pad, gpointer data) {
	// GstPad *sinkpad;
	FilesrcTask *pTask = (FilesrcTask *)data;
	g_print("video decoder received new pad: '%s' from '%s':\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(element));

	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;
	new_pad_caps = gst_pad_get_current_caps(pad);
	new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
	new_pad_type = gst_structure_get_name(new_pad_struct);
	printf("---------------------- pad type: %s\n", new_pad_type);

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

// appsink query
GstPadProbeReturn FilesrcTask::appsink_query_cb(GstPad * pad G_GNUC_UNUSED, GstPadProbeInfo * info, gpointer user_data G_GNUC_UNUSED)
{
	GstQuery *query = (GstQuery *)info->data;

	if (GST_QUERY_TYPE(query) != GST_QUERY_ALLOCATION)
		return GST_PAD_PROBE_OK;

	gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);

	return GST_PAD_PROBE_HANDLED;
}

// bus
gboolean FilesrcTask::onBusMessage(GstBus * bus, GstMessage * msg, gpointer user_data)
{
	FilesrcTask *pTask = (FilesrcTask *)user_data;


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
		//printf("redistributing latency\n");
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

int cpNV12_1(unsigned char* dst, unsigned char* src, int width, int height, int padlen) {

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

GstFlowReturn FilesrcTask::on_video_sample(GstElement* sink, gpointer ptr) {

	FilesrcTask * pTask = (FilesrcTask*)ptr;
	GstSample* videoSample;

	//puts("----- on video =====");
	g_signal_emit_by_name(sink, "pull-sample", &videoSample);

	while (videoSample) {

		GstBuffer* buf = gst_sample_get_buffer(videoSample);
		GstVideoMeta *meta = gst_buffer_get_video_meta(buf);
		guint nplanes = GST_VIDEO_INFO_N_PLANES(&(pTask->videoinfo_));
		GstVideoFormat pixfmt = GST_VIDEO_INFO_FORMAT(&(pTask->videoinfo_));
		const char *pixfmt_str = gst_video_format_to_string(pixfmt);

		//printf("----------video pts:%ld ,  dts=%ld----------\n", GST_BUFFER_PTS(buf), GST_BUFFER_DTS(buf));


		GstMapInfo map_info;
		if (!gst_buffer_map(buf, &map_info, (GstMapFlags)GST_MAP_READ))
		{
			g_print("gst_buffer_map() error!---eos \n");
			break;
		}

		int source_width = GST_VIDEO_INFO_WIDTH(&(pTask->videoinfo_));
		int source_height = GST_VIDEO_INFO_HEIGHT(&(pTask->videoinfo_));


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
			DebugLog::writeLogF("Rtsp Task video stream information:\n");
			DebugLog::writeLogF("  taskid: %s\n", pTask->pFlow_->taskid_.c_str());
			DebugLog::writeLogF("  rtspurl: %s\n", pTask->pFlow_->rtspUrl_.c_str());
			DebugLog::writeLogF("  start time: %d\n", pTask->pFlow_->task_start_time_);
			DebugLog::writeLogF("  size: %u x %u pixel\n", source_width, source_height);
			DebugLog::writeLogF("  pixel format: %s  number of planes: %u\n", pixfmt_str, nplanes);
			//	printf("  video meta found: %d\n", (meta != NULL));
			DebugLog::writeLogF("  mpp frame size : %d \n", map_info.size);
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


		if (pTask->pFlow_->pRawData_in->size() < 5) {

			//	puts("==================================one frame==============");
			//	FILE * yuvout = fopen("tstYuv0518.yuv", "w");
			//	fwrite(map_info.data, map_info.size, 1, yuvout);
			//	fclose(yuvout);

			Packet* newData;

			if (TaskManager::GetInstance()->chip_type_ == CHIP_SOFTWARE) {
				newData = new Packet(map_info.data, map_info.size);
				newData->size = map_info.size;
				newData->type_ = 1;//video
				newData->width = source_width;
				newData->height = source_height;
			}
			else
				if (TaskManager::GetInstance()->chip_type_ == CHIP_ROCKCHIP || TaskManager::GetInstance()->chip_type_ == CHIP_NVV) {
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
								int yuvsize = cpNV12_1(pTmp, map_info.data, source_width, source_height, padlen);
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
						if (map_info.size == 3760128) {
							int padlen = 384;
							unsigned char* pTmp = (unsigned char*)malloc(1920 * 1088 * 3 / 2);
							int yuvsize = cpNV12_1(pTmp, map_info.data, 1920, 1088, padlen);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);

							newData->width = 1920;
							newData->height = 1088;

						}
						else if (map_info.size == 3133440 || map_info.size == 4177920) {// h264 16bit 1920 * 1080 == 1920 * 1088
							newData = new Packet(map_info.data, map_info.size);
							newData->size = map_info.size;
							newData->type_ = 1;//video
							newData->width = 1920;
							newData->height = 1088;
						}
						else if (map_info.size == 8146944) {// 2880 * 1620 2880 * 1632
							int padlen = 448;
							unsigned char* pTmp = (unsigned char*)malloc(2880 * 1620 * 3 / 2);
							int yuvsize = cpNV12_1(pTmp, map_info.data, 2880, 1620, padlen);
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
							int yuvsize = cpNV12_1(pTmp, map_info.data, 1920, 1080, padlen);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);
							newData->width = 1920;
							newData->height = 1080;
						}
						else if (map_info.size == 6082560 || map_info.size == 8110080) {// h265 2560 * 1440 256 2816 1584
							int padlen = 256;
							unsigned char* pTmp = (unsigned char*)malloc(2560 * 1440 * 3 / 2);
							int yuvsize = cpNV12_1(pTmp, map_info.data, 2560, 1440, padlen);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);
							newData->width = 2560;
							newData->height = 1440;
						}
						else if (map_info.size == 6420480) {//  2688 1520
							int padlen = 128;
							unsigned char* pTmp = (unsigned char*)malloc(2688 * 1520 * 3 / 2);
							int yuvsize = cpNV12_1(pTmp, map_info.data, 2688, 1520, padlen);
							newData = new Packet(pTmp, yuvsize);
							newData->size = yuvsize;
							free(pTmp);
							newData->width = 2688;
							newData->height = 1520;
						}

						else if (map_info.size == 8626176) {// 3072 * 1728 
							int padlen = 256;
							unsigned char* pTmp = (unsigned char*)malloc(3072 * 1728 * 3 / 2);
							int yuvsize = cpNV12_1(pTmp, map_info.data, 3072, 1728, padlen);
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
			//		printf("			++++++++++++++++++++++  new video raw :%d  seq=%d\n", ts_time::currentpts(pTask->pFlow_->task_start_time_), newData->seq_);
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

GstFlowReturn FilesrcTask::on_audio_sample(GstElement* sink, gpointer ptr) {

	FilesrcTask * pTask = (FilesrcTask*)ptr;
	GstSample* audioSample;
//	puts("--------------------=== on audio ===");


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
		//	printf("audio frame info----------%d\n", map_info.size);
	//	printf("------audio pts info   %lld\n", GST_BUFFER_PTS(buf));
		if (pTask->pFlow_->pAudioData_in->size() < 50 && map_info.size == 2560) {

			Packet* newData = new Packet(map_info.data, map_info.size);
			newData->type_ = 2;//audio
			newData->size = map_info.size;
			newData->pts_ = GST_BUFFER_PTS(buf);
			pTask->pFlow_->pAudioData_in->push(newData);
			//puts("--------push audio sample-----\n");
		}
		gst_buffer_unmap(buf, &map_info);
		gst_sample_unref(audioSample);
	}
	return GST_FLOW_OK;


}

