

#include "EncodingTask.h"
#include "TaskManager.h"
#include "../common/ts_time.h"
#include "TaskFlow.h"
#include "GstUtil.h"
#include <gst/pbutils/encoding-profile.h>
#include "../common/ts_time.h"
#include "define.h"
#include "../common/EdUrlParser.h"
#include "DebugLog.h"
#define NO_ENHANCE
EncodingTask::EncodingTask(TaskFlow* pFlow)
{
	static int enctid = 0;
	tid_ = enctid++;
	pFlow_ = pFlow;
	withaudio_ = pFlow->withaudio_;
	puts("create EncodingTask");

}

EncodingTask::~EncodingTask() {
	puts("~delete EncodingTask");
	if (pfile != NULL) {
		fclose(pfile);
	}
	reportStatus();

	pFlow_->onComplete(1);
}

void EncodingTask::reportStatus()
{
	TSJson::Value root;
	root["tid"] = pFlow_->taskid_;
	root["taskname"] = "encoding";
	root["starttime"] = pFlow_->task_start_time_;
	if (closestyle_ == 0 || closestyle_ == 1) {
		root["code"] = ERR_NO_ERROR;
	}
	else {
		root["code"] = ERR_ENODE_ERR;
	}

	root["tmstamp"] = ts_time::current();
	if (closestyle_ == 0 || closestyle_ == 1)
		root["message"] = "eos";
	else if (closestyle_ == 2) {
		if (statusSent_ == 1)
			return;
		root["code"] = ERR_RTSP_PUSH_FAIL;
		root["message"] = closeMessage_;
		statusSent_ = 1;
	}
	else if (closestyle_ == 3)
		root["message"] = "end of stream from rtsp source";
	else if (closestyle_ == 4)
	{
		root["code"] = ERR_RTSP_LOST;
		root["message"] = "source stream time out";
	}
	else if (closestyle_ == 5)
		root["message"] = "encoding pipeline create failed,check gstreamer on your device!";
	else if (closestyle_ == 7) {
		
		root["code"] = ERR_RTSP_NET_TIMEOUT;
		root["message"] = "source net link time out";
	}


	puts("============report 2 callback================");
	//if (statusSent_ == 0)
	TaskManager::GetInstance()->onAnyWorkStatus(root, 0);
}

void *EncodingTask::startThread(TaskFlow* pFlow)
{
	EncodingTask* thread = new EncodingTask(pFlow);

	thread->setAutoDelete(true);
	thread->start();

	return thread;
}

bool EncodingTask::onStart()
{
	if (pFlow_->outputtype_ == "rtp") {
		parseSDP();
	}
	return true;
}
void EncodingTask::stop(int type)
{
	closestyle_ = type;// user close
	if (isStart_ > 0)
	{
		int ret;

		if (withaudio_ > 0 && pFlow_->haveaudio_ > 0) {
			g_signal_emit_by_name(audioappsrc_, "end-of-stream", &ret);
		}

		g_signal_emit_by_name(videoappsrc_, "end-of-stream", &ret);
		//if(!bus_)
		//ret1 = gst_bus_post(bus_, gst_message_new_eos(GST_OBJECT_CAST(pipeline_)));

	}
}

int EncodingTask::parseSDP() {
	setup_type_ = 0;//1, active 0,passive
	passive_trans_ = 0; //0 udp, 1 tcp
	listen_port = 8000;
	peer_port = 10004;
	peer_host = "192.168.2.156";
	video_codec_ = "H264";
	if (pFlow_->out_rtp_["setup"].isString())
	{
		string tmp = pFlow_->out_rtp_["setup"].asString();
		setup_type_ = tmp == "active" ? 1 : 0;
	}
	if (pFlow_->out_rtp_["transport"].isString())
	{
		string tmp = pFlow_->out_rtp_["transport"].asString();
		passive_trans_ = tmp == "tcp" ? 1 : 0;
	}
	if (pFlow_->out_rtp_["host"].isString())
	{
		string tmp = pFlow_->out_rtp_["host"].asString();
		peer_host = tmp;
		pFlow_->outputpath_ = peer_host;
	}
	if (pFlow_->out_rtp_["port"].isInt())
	{
		peer_port = pFlow_->out_rtp_["port"].asInt();
	}
	if (pFlow_->out_rtp_["video_pt"].isInt())
	{
		video_pt = pFlow_->out_rtp_["video_pt"].asInt();
	}
	if (pFlow_->out_rtp_["audio_pt"].isInt())
	{
		peer_port = pFlow_->out_rtp_["audio_pt"].asInt();
	}
	if (pFlow_->out_rtp_["ssrc"].isString())
	{
		ssrc_ = pFlow_->out_rtp_["ssrc"].asString();
	}
	if (setup_type_ == 0) {
		listen_port = peer_port;
	}
	return 0;
}
void EncodingTask::run()
{
	gst_init(NULL, NULL);
	while (1) {
		Packet* frame = pFlow_->pRawData->trypop(10);
	
		if (frame != NULL && frame->size > 0) {
			//puts("read first frame");
			curWidth = frame->width;
			curHeight = frame->height;
			delete frame;
			//#ifdef MAKE_SWCODEC
			//			puts("sw codec---------------------pipeline");
			//			if (0 != buildEncodebinTest()) {
			//
			//			}
			//#else
			int ret = 0;
			if (pFlow_->outputtype_ == "rtp") {
				ret = buildRtpPipeline();
			}
			else if (pFlow_->outputtype_ == "rtsp") {
				ret = buildRtspPipeline();
			}
			else if (pFlow_->outputtype_ == "rtmp") {
				ret = buildRtmpPipeline();
			}
			else if (pFlow_->outputtype_ == "raw")
			{
				ret = buildRawPipeline();
			}
			else {
				ret = buildFilePipeline();
			}
			if (0 != ret) {
				{
					puts("encoding task create failed!................");
					closeMessage_ = "build pipeline fail, check gstreamer plugin on the device";
					closestyle_ = 5;
				}
			}
			else {
				if (TaskManager::GetInstance()->chip_type_ == CHIP_ROCKCHIP) {
					puts("encoding pipeline with rockchip HW start working ......");
				}
				else if (TaskManager::GetInstance()->chip_type_ == CHIP_SOFTWARE || TaskManager::GetInstance()->chip_type_ == CHIP_TRYCPU) {
					puts("encoding pipeline with software(x264/x265) start working ......");

				}
				else if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV && pFlow_->enc_sel_ != 0) {
					puts("encoding pipeline with nvv4l2 plugin start working ......");
				}
				else {
					puts("encoding pipeline with software start working ......");
				}
				isStart_ = 1;

				/*bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));:
				gst_element_set_state(pipeline_, GST_STATE_PLAYING);
				do {
					GstMessage* msg = gst_bus_timed_pop_filtered(bus_, 200*GST_MSECOND, (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
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
							g_print("End-Of-Stream in encoding.\n");
							terminate = TRUE;
							break;
						case GST_MESSAGE_STATE_CHANGED:

							if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline_)) {
								GstState old_state, new_state, pending_state;
								gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

							}
							break;
						default:

							g_printerr("Unexpected message received.\n");
							break;
						}
						gst_message_unref(msg);
					}
				} while (!terminate);

				gst_object_unref(bus_);
				gst_element_set_state(pipeline_, GST_STATE_NULL);
				gst_object_unref(pipeline_);*/

				bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
				gst_bus_add_watch(bus_, EncodingTask::onBusMessage, this);
				gst_element_set_state(pipeline_, GST_STATE_PLAYING);
				loop_ = g_main_loop_new(NULL, FALSE);
				g_main_loop_run(loop_);
				gst_object_unref(bus_);
				gst_element_set_state(pipeline_, GST_STATE_NULL);
				gst_object_unref((pipeline_));
				g_main_loop_unref(loop_);
			}

			{
				printf("****************************************************\n");
				printf("Encoding Task complete ...\n");
				printf("  taskid: %s\n", pFlow_->taskid_.c_str());
				printf("  rtspurl: %s\n", pFlow_->rtspUrl_.c_str());
				printf("  end time: %d\n", ts_time::current());
				printf("  duration: %d\n", (ts_time::current()) - pFlow_->task_start_time_);

				printf("****************************************************\n\n");
			}

			return;
		}
		else {
			DebugLog::writeLogF("closed because did not recv frame data in 10 s:%s\n", pFlow_->rtspUrl_.c_str());
			closestyle_ = 4; // timeout
			return;
		}

		ts_time::wait(500);
		continue;
	}
	return;
}

gboolean EncodingTask::onBusMessage(GstBus *bus, GstMessage *msg, gpointer data)
{
	EncodingTask * pTask = (EncodingTask *)data;
	
	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_APPLICATION:
		DebugLog::writeLogF("app msgEncoding task End of stream\n");
		break;
	case GST_MESSAGE_EOS:
		DebugLog::writeLogF("Encoding task End of stream\n");
		g_main_loop_quit(pTask->loop_);
		return false;
		break;

	case GST_MESSAGE_ERROR: {
		gchar *debug;
		GError *error;

		gst_message_parse_error(msg, &error, &debug);
		g_free(debug);

		g_printerr("EncodingTask Error: %s\n", error->message);
		pTask->closeMessage_ = error->message;
		g_error_free(error);
		pTask->closestyle_ = 2; //rtsperr
		if (pTask->statusSent_ == 0) {
			pTask->reportStatus();
		}
		g_main_loop_quit(pTask->loop_);
		return false;
		break;
	}
	default:
		break;
	}
	return true;
}

bool EncodingTask::getAudioFrame()
{
	
	for (int i = 0; i < 3; i++) {
		if (pFlow_->pAudioData->size() == 0) {
			return true;
		}
		Packet* frame = pFlow_->pAudioData->pop();
		if (frame != NULL && frame->len_ == 0) {
			stop(1);
			return false;
		}
		while (frame != NULL && frame->len_ > 0) {

			GstBuffer *buffer;
			GstMapInfo map;
			GstFlowReturn ret;

			if (frame->len_ == 0) {
				g_signal_emit_by_name(audioappsrc_, "end-of-stream", &ret);
				if (closestyle_ == 0) closestyle_ = 3; //end from rtsp
				break;
			}

			buffer = gst_buffer_new_and_alloc(frame->size);

			audio_frame_count_ += 1;
	

			GST_BUFFER_PTS(buffer) = frame->pts_;
			//GST_BUFFER_DTS(buffer) = frame->pts_;
		
		//	printf("----00000000000000000000----dts is %ld  size=%d\n", frame->pts_, frame->size);


			bool bret = gst_buffer_map(buffer, &map, (GstMapFlags)GST_MAP_WRITE);
			if (!bret) {
				DebugLog::writeLogF("-----------------get map error\n");
				return false;
			}
			if (map.size == 0) break;

			memcpy((guint8 *)map.data, frame->pdata_, frame->size);

			gst_buffer_unmap(buffer, &map);

			g_signal_emit_by_name(audioappsrc_, "push-buffer", buffer, &ret);

			gst_buffer_unref(buffer);
			if (ret != GST_FLOW_OK) {
			//	DebugLog::writeLogF("--------------------audio--push error---\n");
				delete frame;
				return false;
			}

			delete frame;
		//	printf("-------------push audio data 6 size=%d fnum=%d\n", frame->size, audio_frame_count_);

			frame = NULL;
			break;
			//return TRUE;

		}
		if (!frame)
			delete frame;
	}

	return true;
	

	return false;

}

bool EncodingTask::getVideoFrame()
{
	/*for (int i = 0; i < 1; i++) {
		if (pFlow_->pRawData->size() == 0) {
			return true;
		}
		*/
		Packet* frame = pFlow_->pRawData->trypop(10);
		if (frame == NULL) {
			stop(7);
			return false;
		}
		if (frame != NULL && frame->len_ == 0) {
			stop(1);
			return false;
		}
		while (frame != NULL && frame->len_ > 0) {

			//puts("read one frame");
			GstBuffer *buffer;
			GstMapInfo map;
			GstFlowReturn ret;

			if (frame->len_ == 0) {
				g_signal_emit_by_name(videoappsrc_, "end-of-stream", &ret);
				delete frame;
				break;
			}

			int eWidth = frame->width;
			int eHeight = frame->height;

			if (eWidth != curWidth || eHeight != curHeight) {
				puts("========================close==================================");
				g_signal_emit_by_name(videoappsrc_, "end-of-stream", &ret);
				//	delete frame;
				closestyle_ = 3;
				reportStatus();
				//	statusSent_ = 1;

				return false;
			}


			buffer = gst_buffer_new_and_alloc(frame->size);

			++video_frame_count_;

			GST_BUFFER_PTS(buffer) = frame->pts_;
			GST_BUFFER_DTS(buffer) = frame->pts_;
			//	printf("                ----11111111111111111111111----dts is %ld %d\n", frame->pts_, frame->seq_);

				// 
				/*if (pFlow_->outputtype_ == "rtsp") {
					GST_BUFFER_TIMESTAMP(buffer) = frame->pts_; (ts_time::currentpts(pFlow_->task_start_time_)) * 1000 * 1000;
				}
				else if (pFlow_->outputtype_ == "rtmp")
				{
					GST_BUFFER_TIMESTAMP(buffer) = (ts_time::currentpts(pFlow_->task_start_time_)) * 1000 * 1000;
					GST_BUFFER_DTS(buffer) = ts_time::currentpts(pFlow_->task_start_time_) * 1000 * 1000;
					GST_BUFFER_PTS(buffer) = ts_time::currentpts(pFlow_->task_start_time_) * 1000 * 1000;
				}
				else

				{
					GST_BUFFER_TIMESTAMP(buffer) = gst_util_uint64_scale(video_frame_count_, GST_SECOND, 25);
					GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(1, GST_SECOND, 25);
				}
				*/
				//	printf("---------------pts  = %ld\n", GST_BUFFER_TIMESTAMP(buffer));
			bool bret = gst_buffer_map(buffer, &map, (GstMapFlags)GST_MAP_WRITE);
			if (!bret) {
				DebugLog::writeLogF("-----------------get map error\n");
				delete frame;
				return false;
			}
			if (map.size == 0) break;

			memcpy((guint8 *)map.data, frame->pdata_, frame->size);

			gst_buffer_unmap(buffer, &map);

			if (terminate) {
				delete frame;
				return false;
			}
			g_signal_emit_by_name(videoappsrc_, "push-buffer", buffer, &ret);
			//	printf("-------------push video data 5  size=%d fnum=%d  buffsize=%d \n", frame->size, video_frame_count_, pFlow_->pRawData->size());

				/*guint curbps = 0;
				g_object_set(G_OBJECT(videoencoder_), "bps", &curbps, NULL);
				guint mxbpx = 0;
				g_object_set(G_OBJECT(videoencoder_), "bps-max", &mxbpx, NULL);

				printf("--------p=%d---------real bps=%d  max=%d--------\n", videoencoder_, curbps, mxbpx);*/


			gst_buffer_unref(buffer);

			if (ret != GST_FLOW_OK) {
				//printf("----------------------push error---\n");
				delete frame;
				return false;
			}

			delete frame;
			frame = NULL;
			return TRUE;
			//break;

		}

		if (!frame)
			delete frame;

	return true;
	return false;

}
gboolean EncodingTask::push_vidoedata(gpointer *data)
{

	EncodingTask * pTask = (EncodingTask*)data;
	bool bret = pTask->getVideoFrame();
	return bret;
}
gboolean EncodingTask::push_audiodata(gpointer *data)
{
	EncodingTask * pTask = (EncodingTask*)data;
	bool bret = pTask->getAudioFrame();
	return bret;
}

static void start_feed(GstElement *source, guint size, gpointer data)
{

	EncodingTask * pTask = (EncodingTask*)data;

	if (g_str_has_prefix(GST_ELEMENT_NAME(source), "videoappsrc")) {
		if (pTask->getVideotimer_ == 0) {
			//		g_print("Start video feeding\n");
			pTask->getVideotimer_ = g_idle_add((GSourceFunc)EncodingTask::push_vidoedata, data);
		}
	}
	else if (g_str_has_prefix(GST_ELEMENT_NAME(source), "audioappsrc")) {
		if (pTask->getAudiotimer_ == 0) {
					g_print("Start audio feeding \n");
			pTask->getAudiotimer_ = g_idle_add((GSourceFunc)EncodingTask::push_audiodata, data);
		}
	}
}

static void stop_feed(GstElement *source, gpointer data)
{
	EncodingTask * pTask = (EncodingTask*)data;


	if (g_str_has_prefix(GST_ELEMENT_NAME(source), "videoappsrc")) {
		if (pTask->getVideotimer_ != 0) {
		//	g_print("Stop video feeding\n");
			g_source_remove(pTask->getVideotimer_);
			pTask->getVideotimer_ = 0;
		}

	}
	else if (g_str_has_prefix(GST_ELEMENT_NAME(source), "audioappsrc")) {
		if (pTask->getAudiotimer_ != 0) {
			g_print("Stop audio feeding\n");
			g_source_remove(pTask->getAudiotimer_);
			pTask->getAudiotimer_ = 0;
		}
	}

}

int EncodingTask::addAudioBranch()
{
	if (withaudio_ > 0 && pFlow_->haveaudio_ > 0) {
		puts("add audio-----------------------0");

		audioappsrc_ = gst_element_factory_make("appsrc", ("audioappsrc" + std::to_string(tid_)).c_str());
		aconverter_ = gst_element_factory_make("audioconvert", ("aconv" + std::to_string(tid_)).c_str());
#if defined(NO_ENHANCE)
#else
		audiocpasfilter_ = gst_element_factory_make("capsfilter", NULL);
		audioresample_ = gst_element_factory_make("audioresample", ("audiorate" + std::to_string(tid_)).c_str());
		if (!audiocpasfilter_) {
			puts("---audio caps filter created failed ---");
		}
#endif
		
		audioencoder_ = gst_element_factory_make("voaacenc", ("audioencoder" + std::to_string(tid_)).c_str());

		if (!audioencoder_) {

			audioencoder_ = gst_element_factory_make("avenc_aac", ("audioencoder" + std::to_string(tid_)).c_str());

		}
		queue2_ = gst_element_factory_make("queue", ("queue2" + std::to_string(tid_)).c_str());
		g_object_set(queue2_, "max-size-buffers", this->pFlow_->buf_output_ * 2, NULL);

		audioparser_ = gst_element_factory_make("aacparse", ("audioparser" + std::to_string(tid_)).c_str());

		if (!audioappsrc_ || !audioencoder_ || !audioparser_ ) {
			DebugLog::writeLogF("audio element create failed 1\n");
			return -1;
		}


		setExtraParameter(audioencoder_, pFlow_->aac_extra_);


#if defined(NO_ENHANCE)
		GstCaps *caps = gst_caps_new_simple("audio/x-raw", \
			"format", G_TYPE_STRING, "S16LE", \
			"layout", G_TYPE_STRING, "interleaved", \
			"rate", G_TYPE_INT, 8000, \
			"channels", G_TYPE_INT, 1, \
			NULL);
#else
		string strFormat = pFlow_->withfilter_ == 0 ? "S16LE" : "F32LE";
		int iRate =  16000;

		puts("add audio-----------------------1");

		GstCaps *caps = gst_caps_new_simple("audio/x-raw", \
			"format", G_TYPE_STRING, strFormat.c_str(), \
			"layout", G_TYPE_STRING, "interleaved", \
			"rate", G_TYPE_INT, iRate, \
			"channels", G_TYPE_INT, 1, \
			NULL);

		/*if (pFlow_->withfilter_ == 0) {
			gst_util_set_object_arg(G_OBJECT(audiocpasfilter_), "caps",
				"audio/x-raw, "
				"format={ S16LE},rate=8000");
		}
		else */
		{
			gst_util_set_object_arg(G_OBJECT(audiocpasfilter_), "caps",
				"audio/x-raw, "
				"format={ S16LE},rate=16000");
		}
	
#endif
		g_object_set(audioencoder_, "perfect-timestamp", false, NULL);
		g_object_set(audioappsrc_, "caps", caps, "format", GST_FORMAT_TIME, NULL);
		g_object_set(audioencoder_, "tolerance", 200000000, NULL);
		gst_caps_unref(caps);
		g_signal_connect(audioappsrc_, "need-data", G_CALLBACK(start_feed), this);
		g_signal_connect(audioappsrc_, "enough-data", G_CALLBACK(stop_feed), this);

#if defined(NO_ENHANCE)
		if (pFlow_->outputtype_ == "ts") {
		gst_bin_add_many(GST_BIN(pipeline_), audioappsrc_, aconverter_, audioencoder_, queue2_, audioparser_, NULL);

		if (!gst_element_link_many(audioappsrc_, aconverter_,  audioencoder_, queue2_, audioparser_, NULL)) {
			g_printerr("audio branch link error. \n");
			return -1;
			}
		}
		else {
			gst_bin_add_many(GST_BIN(pipeline_), audioappsrc_, aconverter_, audioencoder_, queue2_, audioparser_, NULL);
			if (!gst_element_link_many(audioappsrc_, aconverter_, audioencoder_, queue2_, audioparser_, NULL)) {
				g_printerr("audio branch link error. \n");
				return -1;
			}
		}
		puts("-------------no enhance branch\n");
#else
		gst_bin_add_many(GST_BIN(pipeline_), audioappsrc_, aconverter_, audioresample_, audiocpasfilter_, audioencoder_, queue2_, audioparser_, NULL);

		if (!gst_element_link_many(audioappsrc_, aconverter_,audioresample_, audiocpasfilter_,audioencoder_, queue2_, audioparser_, NULL)) {
			g_printerr("audio branch link error. \n");
			return -1;
		}
		puts("-------------with enhance branch\n");
#endif
		GstPadTemplate *mux_src_pad_template;
		if (pFlow_->outputtype_ != "ts") {
			mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(avmuxer_), "audio_%u");
		if (mux_src_pad_template == NULL)		g_printerr("mux_src_pad_template null. \n");

		GstPad * muxer_pad = gst_element_request_pad(avmuxer_, mux_src_pad_template, NULL, NULL);
		if (muxer_pad == NULL)		g_printerr("muxer_pad null. \n");

		GstPad * parser_pad = gst_element_get_static_pad(audioparser_, "src");
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

			GstPad * muxer_pad = gst_element_request_pad(avmuxer_, mux_src_pad_template, NULL, NULL);
			if (muxer_pad == NULL)		g_printerr("muxer_pad null. \n");
			GstPad * parser_pad = gst_element_get_static_pad(audioparser_, "src");
			if (parser_pad == NULL)		g_printerr("parser_pad null. \n");
			if (gst_pad_link(parser_pad, muxer_pad) != GST_PAD_LINK_OK) {
				g_printerr("audio parse-mux link error. \n");
				return -1;
			}
			gst_object_unref(parser_pad);
			gst_object_unref(muxer_pad);
		}
	}
	return 0;
}
int EncodingTask::selectVideoEncoder()
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
			g_object_set(G_OBJECT(videoencoder_), "bps", pFlow_->bitrate_ * 1024*10, NULL);
			g_object_set(G_OBJECT(videoencoder_), "bps-max", pFlow_->bitrate_ * 1024*10, NULL);
			g_object_set(G_OBJECT(videoencoder_), "gop", pFlow_->gop_, NULL);
			//g_object_set(G_OBJECT(videoencoder_), "profile", pFlow_->profile_, NULL);
			g_object_set(G_OBJECT(videoencoder_), "rc-mode", pFlow_->rcmode_, NULL);

			guint curbps = 0;
			g_object_get(G_OBJECT(videoencoder_), "bps", &curbps, NULL);
			guint mxbpx = 0;
			g_object_get(G_OBJECT(videoencoder_), "bps-max", &mxbpx, NULL);


			//	printf("==================mpp 264 read============= bps=%d  max=%d\n", curbps, mxbpx);

		}
		else if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV && pFlow_->enc_sel_ !=0) {
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
		else 
			if (TaskManager::GetInstance()->chip_type_ == CHIP_NVV) {
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
			default_fps_n_254_ = 25;
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


int EncodingTask::buildFilePipeline()
{

	pipeline_ = gst_pipeline_new(("encode_pipeline" + std::to_string(tid_)).c_str());
	videoappsrc_ = gst_element_factory_make("appsrc", ("videoappsrc" + std::to_string(tid_)).c_str());

	selectVideoEncoder();
	if (pFlow_->encodetype_ == "h264") {
		videoparser_ = gst_element_factory_make("h264parse", ("parser" + std::to_string(tid_)).c_str());
	}
	else if (pFlow_->encodetype_ == "h265") {
		videoparser_ = gst_element_factory_make("h265parse", ("parser" + std::to_string(tid_)).c_str());
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

	if (!pipeline_ || !videoappsrc_ || !videoencoder_ || !videoparser_ || !avmuxer_ || !streamsink_)
	{
		g_printerr("Not all elements could be created.\n");
		return -1;
	}
	queue1_ = gst_element_factory_make("queue", ("queue1" + std::to_string(tid_)).c_str());

	if (pFlow_->outputtype_ != "ts") {
		g_object_set(avmuxer_, "faststart", TRUE, NULL);
		g_object_set(avmuxer_, "streamable", TRUE, NULL);
		g_object_set(avmuxer_, "presentation-time", true, NULL);
		g_object_set(avmuxer_, "fragment-duration", 1000, NULL);
	}
	g_object_set(queue1_, "max-size-buffers", this->pFlow_->buf_output_, NULL);


	char dstFilename[256] = { 0 };
	//sprintf(dstFilename, "%s/%s_%s_%d.%s", pFlow_->outputpath_.c_str(), pFlow_->taskid_.c_str(), pFlow_->encodetype_.c_str(), time(NULL), FileSuffix.c_str());
	sprintf(dstFilename, "%s", pFlow_->outputpath_.c_str());
	g_object_set(streamsink_, "location", dstFilename, NULL);
	g_object_set(streamsink_, "o-sync", TRUE, NULL);

	if (addVideoSrc() < 0)
	{
		g_printerr("Encoding video src set failed.\n");
		return -1;
	}


	gst_bin_add_many(GST_BIN(pipeline_), videoappsrc_, vconverter_, videoencoder_, queue1_, videoparser_, avmuxer_, streamsink_, NULL);

	if (!gst_element_link_many(videoappsrc_, vconverter_, videoencoder_, queue1_, videoparser_, NULL) || !gst_element_link_many(avmuxer_, streamsink_, NULL)) {
		return -1;
	}

	if (addAudioBranch() < 0) {
		return -1;
	}

	puts("check video-----------------------------");
	GstPadTemplate *mux_src_pad_template;
	if (pFlow_->outputtype_ == "ts") {
		mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(avmuxer_), "sink_%d");
	}
	else {
		mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(avmuxer_), "video_%u");
	}
	if (mux_src_pad_template == NULL)		g_printerr("mux_src_pad_template null. \n");

	GstPad * muxer_pad = gst_element_request_pad(avmuxer_, mux_src_pad_template, NULL, NULL);
	if (muxer_pad == NULL)		g_printerr("muxer_pad null. \n");

	GstPad * parser_pad = gst_element_get_static_pad(videoparser_, "src");
	if (parser_pad == NULL)		g_printerr("parser_pad null. \n");

	if (gst_pad_link(parser_pad, muxer_pad) != GST_PAD_LINK_OK) {
		g_printerr("video parser-muxer link error. \n");
		return -1;
	}
	gst_object_unref(parser_pad);
	gst_object_unref(muxer_pad);

	puts("check video-----------------------------1");

	return 0;

}

void EncodingTask::setExtraParameter(GstElement * pElement, TSJson::Value& JsonObj) {
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

int EncodingTask::addRtspAudioBranch()
{
	if (withaudio_ > 0 && pFlow_->haveaudio_ > 0) {

		audioappsrc_ = gst_element_factory_make("appsrc", ("audioappsrc" + std::to_string(tid_)).c_str());
		//audioresample_ = gst_element_factory_make("audiorate", ("audiorate" + std::to_string(tid_)).c_str());
		aconverter_ = gst_element_factory_make("audioconvert", ("aconv" + std::to_string(tid_)).c_str());
		queue_ = gst_element_factory_make("queue", ("queue21" + std::to_string(tid_)).c_str());
		g_object_set(queue_, "max-size-buffers", this->pFlow_->buf_output_ * 2, NULL);

		queue2_ = gst_element_factory_make("queue", ("queue2" + std::to_string(tid_)).c_str());
		g_object_set(queue2_, "max-size-buffers", this->pFlow_->buf_output_ * 2, NULL);
#if defined(NO_ENHANCE)
#else
		audiocpasfilter_ = gst_element_factory_make("capsfilter", NULL);
		audioresample_ = gst_element_factory_make("audioresample", ("audiorate" + std::to_string(tid_)).c_str());


#endif
		audioencoder_ = gst_element_factory_make("alawenc", ("audioencoder" + std::to_string(tid_)).c_str());
		g_object_set(audioencoder_, "perfect-timestamp", false, NULL);
		g_object_set(audioencoder_, "tolerance", 200000000, NULL);
	//	audioencoder_ = gst_element_factory_make("voaacenc", ("audioencoder" + std::to_string(tid_)).c_str());

		audioparser_ = gst_element_factory_make("aacparse", ("audioparser" + std::to_string(tid_)).c_str());

		if (!audioappsrc_ || !audioencoder_ || !audioparser_ ) {
			DebugLog::writeLogF("audio element create failed\n");
			return -1;
		}
	//	g_object_set(audioparser_, "max-size-buffers", this->pFlow_->queue_size_, NULL);

		setExtraParameter(audioencoder_, pFlow_->aac_extra_);
#if defined(NO_ENHANCE)
		GstCaps *caps = gst_caps_new_simple("audio/x-raw", \
			"format", G_TYPE_STRING, "S16LE", \
			"layout", G_TYPE_STRING, "interleaved", \
			"rate", G_TYPE_INT, 8000, \
			"channels", G_TYPE_INT, pFlow_->audiochannel, \
			NULL);
#else
		string strFormat = pFlow_->withfilter_ == 0 ? "S16LE" : "F32LE";
		int iRate = 16000;

		GstCaps *caps = gst_caps_new_simple("audio/x-raw", \
			"format", G_TYPE_STRING, strFormat.c_str(), \
			"layout", G_TYPE_STRING, "interleaved", \
			"rate", G_TYPE_INT, iRate, \
			"channels", G_TYPE_INT, 1, \
			NULL);
	/*	if (pFlow_->withfilter_ == 0) {
			gst_util_set_object_arg(G_OBJECT(audiocpasfilter_), "caps",
				"audio/x-raw, "
				"format={ S16LE},rate=8000");
		}
		else */
		{
			gst_util_set_object_arg(G_OBJECT(audiocpasfilter_), "caps",
				"audio/x-raw, "
				"format={ S16LE},rate=16000");
		}
#endif
		g_object_set(audioencoder_, "perfect-timestamp", false, NULL);
		g_object_set(audioappsrc_, "caps", caps, "format", GST_FORMAT_TIME, NULL);
		gst_caps_unref(caps);
		g_signal_connect(audioappsrc_, "need-data", G_CALLBACK(start_feed), this);
		g_signal_connect(audioappsrc_, "enough-data", G_CALLBACK(stop_feed), this);

		aaccpasfilter_ = gst_element_factory_make("capsfilter", NULL);
		{
			gst_util_set_object_arg(G_OBJECT(aaccpasfilter_), "caps",
				"audio/mpeg, "
				"rate=16000");
		}


#if defined(NO_ENHANCE)
		gst_bin_add_many(GST_BIN(pipeline_), audioappsrc_, aconverter_, queue_,audioencoder_,  queue2_,  NULL);

		if (!gst_element_link_many(audioappsrc_, aconverter_, queue_,audioencoder_,  queue2_,  streamsink_, NULL)) {
			g_printerr("audio branch link error. \n");
			return -1;
		}
#else
		gst_bin_add_many(GST_BIN(pipeline_), audioappsrc_, aconverter_, audioresample_, audiocpasfilter_,  audioencoder_,  queue2_, audioparser_, NULL);

		if (!gst_element_link_many(audioappsrc_, aconverter_, audioresample_, audiocpasfilter_,  audioencoder_,  queue2_,audioparser_,  streamsink_, NULL)) {
			g_printerr("audio branch link error. \n");
			return -1;
		}
#endif
	}
	return 0;
}

int EncodingTask::addRtpAudioBranch()
{
	if (withaudio_ > 0 && pFlow_->haveaudio_ > 0) {
		audioappsrc_ = gst_element_factory_make("appsrc", ("audioappsrc" + std::to_string(tid_)).c_str());
		aconverter_ = gst_element_factory_make("audioconvert", ("aconv" + std::to_string(tid_)).c_str());
		queue_ = gst_element_factory_make("queue", ("queue21" + std::to_string(tid_)).c_str());
		g_object_set(queue_, "max-size-buffers", this->pFlow_->buf_output_ * 2, NULL);
		queue2_ = gst_element_factory_make("queue", ("queue2" + std::to_string(tid_)).c_str());
		g_object_set(queue2_, "max-size-buffers", this->pFlow_->buf_output_ * 2, NULL);
		audioencoder_ = gst_element_factory_make("alawenc", ("audioencoder" + std::to_string(tid_)).c_str());
		audioparser_ = gst_element_factory_make("rtppcmapay", ("rtppcmapay" + std::to_string(tid_)).c_str());
		streamsink1_ = gst_element_factory_make("udpsink", ("udpsink_a" + std::to_string(tid_)).c_str()); // rtpbin + udpsink | tcpclientsink tcpclientsink
		if (!audioappsrc_ || !audioencoder_ || !audioparser_) {
			DebugLog::writeLogF("audio element create failed\n");
			return -1;
		}
		g_object_set(GST_OBJECT(streamsink1_), "host", pFlow_->outputpath_.c_str(), NULL);
		g_object_set(GST_OBJECT(streamsink1_), "port", peer_port, NULL);
		setExtraParameter(audioencoder_, pFlow_->aac_extra_);
		GstCaps *caps = gst_caps_new_simple("audio/x-raw", \
			"format", G_TYPE_STRING, "S16LE", \
			"layout", G_TYPE_STRING, "interleaved", \
			"rate", G_TYPE_INT, 8000, \
			"channels", G_TYPE_INT, pFlow_->audiochannel, \
			NULL);
		g_object_set(audioencoder_, "perfect-timestamp", false, NULL);
		g_object_set(audioappsrc_, "caps", caps, "format", GST_FORMAT_TIME, NULL);
		gst_caps_unref(caps);
		g_signal_connect(audioappsrc_, "need-data", G_CALLBACK(start_feed), this);
		g_signal_connect(audioappsrc_, "enough-data", G_CALLBACK(stop_feed), this);
		GstPadTemplate *mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(avmuxer_), "send_rtp_sink_%u");
		if (mux_src_pad_template == NULL)		g_printerr("recv_rtp_sink_pad_template null in audio. \n");
		GstPad * muxer_pad = gst_element_request_pad(avmuxer_, mux_src_pad_template, NULL, NULL);
		if (muxer_pad == NULL)		g_printerr("muxer_pad null in audio. \n");
		gst_object_unref(muxer_pad);
		gst_bin_add_many(GST_BIN(pipeline_), audioappsrc_, aconverter_, queue_, audioencoder_, audioparser_, streamsink1_, NULL);
		if (!gst_element_link_many(audioappsrc_, aconverter_, queue_, audioencoder_, audioparser_, avmuxer_, streamsink1_, NULL)) {
			g_printerr("audio branch link error. \n");
			return -1;
		}
	}
	return 0;
}
int EncodingTask::addVideoSrc()
{
	
	string strFormat;
	if (pFlow_->withfilter_ == 1)
		strFormat = "I420";
	else
	{
		strFormat = pFlow_->pixel_fmt_;
	}

	printf("============src fps:n=%d d=%d\n", pFlow_->src_fs_n_, pFlow_->src_fs_d_);
	int fps_n = 0;
	int fps_d = pFlow_->src_fs_d_;
	if (pFlow_->src_fs_n_ == 0) {
		if (default_fps_n_254_ != 0) {
			fps_n = default_fps_n_254_;
		}
	}
	else {
		fps_n = pFlow_->src_fs_n_;
	}
	if (fps_d != 1) {
		if (fps_n != 25) {
			fps_n = fps_n / fps_d;
		}
		fps_d = 1;

	}

	//fps_n = 25;
	printf("encod video type = %s w=%d h=%d fps=%d %d\n", strFormat.c_str(), curWidth, curHeight, fps_n, pFlow_->src_fs_d_);
	GstCaps *caps = gst_caps_new_simple("video/x-raw", \
		"format", G_TYPE_STRING, strFormat.c_str(), \
		"interlace-mode", G_TYPE_STRING, "progressive", \
		"multiview-mode", G_TYPE_STRING, "mono", \
		"chroma-site", G_TYPE_STRING, "mpeg2", \
		"width", G_TYPE_INT, curWidth, \
		"height", G_TYPE_INT, curHeight, \
		"framerate", GST_TYPE_FRACTION, fps_n , fps_d, \
		NULL);

	g_object_set(videoappsrc_, "caps", caps, "format", GST_FORMAT_TIME, NULL);
	gst_caps_unref(caps);
	g_signal_connect(videoappsrc_, "need-data", G_CALLBACK(start_feed), this);
	g_signal_connect(videoappsrc_, "enough-data", G_CALLBACK(stop_feed), this);

	return 0;
}

static void new_manager_callback(GstElement * rtsp_client_sink,
	GstElement * manager,
	gpointer udata) {
	printf("=============on new rtsp element====  %s ========\n", GST_ELEMENT_NAME(manager));
	g_object_set(G_OBJECT(manager), "latency", 2000, NULL);
	g_object_set(G_OBJECT(manager), "buffer-mode", 2, NULL);
//	g_object_set(G_OBJECT(manager), "faststart-min-packets", 5, NULL);
	g_object_set(G_OBJECT(manager), "drop-on-latency", true, NULL);
	
	
}

static void new_payloader_callback(GstElement * rtsp_client_sink,
	GstElement * payloader,
	gpointer udata) {
	printf("=============on new rtsp payloader ============\n");
}

int EncodingTask::buildRtspPipeline()
{

	pipeline_ = gst_pipeline_new(("encode_pipeline" + std::to_string(tid_)).c_str());
	videoappsrc_ = gst_element_factory_make("appsrc", ("videoappsrc" + std::to_string(tid_)).c_str());

	selectVideoEncoder();
	queue1_ = gst_element_factory_make("videorate", ("prequeue" + std::to_string(tid_)).c_str());
	videoparser_ = gst_element_factory_make("queue", ("videoqueue" + std::to_string(tid_)).c_str());
	streamsink_ = gst_element_factory_make("rtspclientsink", ("sink" + std::to_string(tid_)).c_str());
	cpasfilter_ = gst_element_factory_make("capsfilter", NULL);
	if (!cpasfilter_) {
		puts("--- caps filter created failed ---");
	}

	/*gst_util_set_object_arg(G_OBJECT(cpasfilter_), "caps",
		"audio/x-raw, "
		"framerate=25/1");*/

	GstCaps* caps = gst_caps_new_simple("video/x-raw", \
		"framerate", GST_TYPE_FRACTION, 25, 1, \
		NULL);

	g_object_set(cpasfilter_, "caps", caps, NULL);
	gst_caps_unref(caps);

	if (!streamsink_) {
		g_printerr("rtspclientsink could be created.\n");

	}

	if (!pipeline_ || !vconverter_ || !videoappsrc_ || !videoencoder_ || !videoparser_ || !streamsink_)
	{
		g_printerr("Not all elements could be created.\n");
		return -1;
	}

	g_object_set(videoparser_, "max-size-buffers", this->pFlow_->buf_output_ *2, NULL);

	//char dstFilename[256] =  "rtsp://172.30.20.244:8554/mystream";
	EdUrlParser* url = EdUrlParser::parseRtspUrl(pFlow_->outputpath_);

	puts(pFlow_->outputpath_.c_str());
	g_object_set(streamsink_, "location", pFlow_->outputpath_.c_str(), NULL);
	g_object_set(GST_OBJECT(streamsink_), "latency", pFlow_->delay_output_, NULL);
	g_object_set(GST_OBJECT(streamsink_), "rtp-blocksize", 576, NULL);
//	g_object_set(GST_OBJECT(streamsink_), "debug", true, NULL);
	g_object_set(GST_OBJECT(streamsink_), "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);

	g_object_set(GST_OBJECT(streamsink_), "rtx-time", 2000, NULL);
	if (!url->authUser_.empty()) {
		g_object_set(GST_OBJECT(streamsink_), "user-id", url->authUser_.c_str(), NULL);
		g_object_set(GST_OBJECT(streamsink_), "user-pw", url->authPswd_.c_str(), NULL);
	}
	//g_object_set(GST_OBJECT(streamsink_), "publish-clock-mode", 0, NULL);
	//publish-clock-mode

	//	g_object_set(streamsink_, "debug", true, NULL);
	if (addVideoSrc() < 0)
	{
		g_printerr("Encoding video src set failed.\n");
		return -1;
	}


	gst_bin_add_many(GST_BIN(pipeline_), videoappsrc_,  vconverter_,  videoencoder_, videoparser_,  streamsink_, NULL);

	if (!gst_element_link_many(videoappsrc_, vconverter_,  videoencoder_, videoparser_,  streamsink_, NULL)) {

		puts("================link error");
		return -1;
	}

	GstPad *pad = gst_element_get_static_pad(videoencoder_, "src");
	gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)cb_have_data, this, NULL);
	gst_object_unref(pad);

	g_signal_connect(streamsink_, "handle-request", G_CALLBACK(handle_request_callback), this);
//	g_signal_connect(streamsink_, "new-payloader", G_CALLBACK(new_payloader_callback), this);
	//g_signal_connect(streamsink_, "new-manager", G_CALLBACK(new_manager_callback), this);





	if (addRtspAudioBranch() < 0) {
		return -1;
	}

	return 0;

}
int EncodingTask::buildRtpPipeline()
{
	pipeline_ = gst_pipeline_new(("encode_pipeline" + std::to_string(tid_)).c_str());
	videoappsrc_ = gst_element_factory_make("appsrc", ("videoappsrc" + std::to_string(tid_)).c_str());
	selectVideoEncoder();
	videoparser_ = gst_element_factory_make("rtph264pay", ("rtph264pay" + std::to_string(tid_)).c_str());
	avmuxer_ = gst_element_factory_make("rtpbin", ("rtpbin" + std::to_string(tid_)).c_str());
	streamsink_ = gst_element_factory_make("udpsink", ("udpsink" + std::to_string(tid_)).c_str()); // rtpbin + udpsink | tcpclientsink tcpclientsink
	if (!streamsink_) {
		g_printerr("rtpclientsink could be created.\n");
	}
	if (!pipeline_ || !vconverter_ || !videoappsrc_ || !videoencoder_ || !videoparser_ || !streamsink_)
	{
		g_printerr("Not all elements could be created.\n");
	}
	puts(pFlow_->outputpath_.c_str());
	g_object_set(GST_OBJECT(streamsink_), "host", pFlow_->outputpath_.c_str(), NULL);
	g_object_set(GST_OBJECT(streamsink_), "port", 8000, NULL);
	if (addVideoSrc() < 0)
	{
		g_printerr("Encoding video src set failed.\n");
		return -1;
	}
	GstPadTemplate *mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(avmuxer_), "send_rtp_sink_%u");
	if (mux_src_pad_template == NULL)		g_printerr("recv_rtp_sink_pad_template null. \n");
	GstPad * muxer_pad = gst_element_request_pad(avmuxer_, mux_src_pad_template, NULL, NULL);
	if (muxer_pad == NULL)		g_printerr("muxer_pad null. \n");
	gst_object_unref(muxer_pad);
	gst_bin_add_many(GST_BIN(pipeline_), videoappsrc_, vconverter_, videoencoder_, videoparser_, avmuxer_, streamsink_, NULL);
	if (pFlow_->encodetype_ == "h264") {
		gst_bin_add_many(GST_BIN(pipeline_), videocpasfilter_, NULL);
		if (!gst_element_link_many(videoappsrc_, vconverter_, videoencoder_, videocpasfilter_, videoparser_, avmuxer_, streamsink_, NULL)) {
			puts("rtp ================link error");
			return -1;
		}
		puts("rtp ================link ok");
	}
	else {
		if (!gst_element_link_many(videoappsrc_, vconverter_, videoencoder_, videoparser_, streamsink_, NULL)) {
			puts("================link error");
			return -1;
		}
	}
	if (addRtpAudioBranch() < 0) {
		return -1;
	}

	return 0;

}

int EncodingTask::buildRawPipeline() {
	pipeline_ = gst_pipeline_new(("encode_pipeline" + std::to_string(tid_)).c_str());
	videoappsrc_ = gst_element_factory_make("appsrc", ("videoappsrc" + std::to_string(tid_)).c_str());

	selectVideoEncoder();
	videoparser_ = gst_element_factory_make("queue", ("videoqueue" + std::to_string(tid_)).c_str());
	streamsink_ = gst_element_factory_make("appsink", ("sink" + std::to_string(tid_)).c_str());
	

	if (!streamsink_) {
		g_printerr("rtspclientsink could be created.\n");

	}

	if (!pipeline_ || !vconverter_ || !videoappsrc_ || !videoencoder_ || !videoparser_ || !streamsink_)
	{
		g_printerr("Not all elements could be created.\n");
		return -1;
	}

	g_object_set(videoparser_, "max-size-buffers", this->pFlow_->buf_output_, NULL);

	
	g_object_set(G_OBJECT(streamsink_), "sync", true, NULL);
	

	gst_base_sink_set_max_lateness(GST_BASE_SINK(streamsink_), 70 * GST_MSECOND);
	gst_base_sink_set_qos_enabled(GST_BASE_SINK(streamsink_), TRUE);
	g_object_set(G_OBJECT(streamsink_), "max-buffers", this->pFlow_->buf_output_/2, NULL);
	g_object_set(G_OBJECT(streamsink_), "emit-signals", TRUE, NULL);
	g_signal_connect(G_OBJECT(streamsink_), "new-sample", G_CALLBACK(EncodingTask::on_video_frame), this);
	gst_app_sink_set_max_buffers(GST_APP_SINK(streamsink_), 1);
	gst_app_sink_set_buffer_list_support(GST_APP_SINK(streamsink_), FALSE);


	if (addVideoSrc() < 0)
	{
		g_printerr("Encoding video src set failed.\n");
		return -1;
	}


	gst_bin_add_many(GST_BIN(pipeline_), videoappsrc_, vconverter_, videoencoder_, videoparser_, streamsink_, NULL);

	if (!gst_element_link_many(videoappsrc_, vconverter_, videoencoder_, videoparser_, streamsink_, NULL)) {

		puts("================link error");
		return -1;
	}

	return 0;
}

GstFlowReturn EncodingTask::on_video_frame(GstElement* sink, gpointer ptr)
{

	//printf("==================on video frame====================111\n");
	EncodingTask* pTask = (EncodingTask*)ptr;
	GstSample* videoSample;

	g_signal_emit_by_name(sink, "pull-sample", &videoSample);
	if (videoSample) {

		if (pTask->pfile == NULL) {
			puts(pTask->pFlow_->outputpath_.c_str());
			pTask->pfile = fopen(pTask->pFlow_->outputpath_.c_str(),"w");
		}

		GstBuffer* buf = gst_sample_get_buffer(videoSample);
		//GstVideoMeta* meta = gst_buffer_get_video_meta(buf);
		GstMapInfo map_info;
		if (!gst_buffer_map(buf, &map_info, (GstMapFlags)GST_MAP_READ))
		{
			g_print("gst_buffer_map() error!---eos \n");
			return GST_FLOW_OK;
		}

		fwrite(map_info.data, map_info.size,1, pTask->pfile);

		gst_buffer_unmap(buf, &map_info);
		gst_sample_unref(videoSample);

	}

	return GST_FLOW_OK;
}

int EncodingTask::buildRtmpPipeline()
{

	pipeline_ = gst_pipeline_new(("encode_pipeline" + std::to_string(tid_)).c_str());
	videoappsrc_ = gst_element_factory_make("appsrc", ("videoappsrc" + std::to_string(tid_)).c_str());
	selectVideoEncoder();
	streamsink_ = gst_element_factory_make("rtmp2sink", ("sink" + std::to_string(tid_)).c_str());
	avmuxer_ = gst_element_factory_make("flvmux", ("muxer" + std::to_string(tid_)).c_str());

	if (pFlow_->encodetype_ == "h264") {
		videoparser_ = gst_element_factory_make("h264parse", ("parser" + std::to_string(tid_)).c_str());
	}
	else if (pFlow_->encodetype_ == "h265") {
		videoparser_ = gst_element_factory_make("h265parse", ("parser" + std::to_string(tid_)).c_str());
	}
	if (!pipeline_ || !videoappsrc_ || !videoencoder_ || !videoparser_ || !avmuxer_ || !streamsink_)
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
	if (addVideoSrc() < 0)
	{
		g_printerr("Encoding video src set failed.\n");
		return -1;
	}

	gst_bin_add_many(GST_BIN(pipeline_), videoappsrc_, videoencoder_, videoparser_, avmuxer_, streamsink_, NULL);

	if (!gst_element_link_many(videoappsrc_, videoencoder_, videoparser_, avmuxer_, streamsink_, NULL)) {
		g_printerr("rtmp link error. \n");

		return -1;
	}
	/*if (!gst_element_link_many(videoappsrc_, videoencoder_, videoparser_, NULL) || !gst_element_link_many(avmuxer_, streamsink_, NULL)) {
		return -1;
	}


	if (addRtspAudioBranch() < 0) {
		return -1;
	}

	GstPadTemplate *mux_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(avmuxer_), "video_%u");
	GstPad * muxer_pad = gst_element_request_pad(avmuxer_, mux_src_pad_template, NULL, NULL);
	GstPad * parser_pad = gst_element_get_static_pad(videoparser_, "src");
	if (gst_pad_link(parser_pad, muxer_pad) != GST_PAD_LINK_OK) {
		g_printerr("video parser-muxer link error rtmp. \n");
		return -1;
	}
	gst_object_unref(parser_pad);
	gst_object_unref(muxer_pad);*/

	return 0;

}

GstPadProbeReturn EncodingTask::cb_have_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
	EncodingTask *pTask = (EncodingTask *)user_data;

	GstBuffer *buffer;

	buffer = GST_PAD_PROBE_INFO_BUFFER(info);

	//if (pTask->cur_video_pts_ != buffer->pts) {
//	printf("------------------------------------one video frame after encoding  %d ms  seq=%d\n",  ts_time::currentpts(pTask->pFlow_->task_start_time_), pTask->cur_seq_++);
	//	pTask->cur_video_pts_ = buffer->pts;
	//}

	//	buffer = gst_buffer_make_writable(buffer);
	if (buffer == NULL)
		return GST_PAD_PROBE_OK;
	//	GST_PAD_PROBE_INFO_DATA(info) = buffer;

	return GST_PAD_PROBE_OK;
}

GstEncodingProfile * create_ogg_theora_profile(void)
{
	GstEncodingContainerProfile *prof;
	GstCaps *caps;
	caps = gst_caps_from_string("application/mp4");
	prof = gst_encoding_container_profile_new("mp4video", "h264", caps, NULL);
	gst_caps_unref(caps);

	caps = gst_caps_from_string("video/x-h264");
	gst_encoding_container_profile_add_profile(prof, (GstEncodingProfile*)gst_encoding_video_profile_new(caps, NULL, NULL, 0));
	gst_caps_unref(caps);

	return (GstEncodingProfile*)prof;
}

void EncodingTask::handle_request_callback(GstElement * rtsp_client_sink, GstRTSPMessage * request, GstRTSPMessage * response, gpointer udata)
{
	RtspTask *pTask = (RtspTask *)udata;


	{

		GstRTSPMethod rtspmtd;
		gst_rtsp_message_parse_request(request, &rtspmtd, NULL, NULL);

		DebugLog::writeLogF("------------------------show rtsp methond = %d----\n", rtspmtd);

	}
	return;
}

