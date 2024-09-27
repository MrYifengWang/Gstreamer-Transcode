#include "JpgTranscoding.h"
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

JpgTranscodingTask::JpgTranscodingTask(TaskFlow* pFlow)
{
	pFlow_ = pFlow;
	tid_ = TaskManager::total_tid++;
	gst_init(NULL, NULL);
	puts("create JpgTranscodingTask");
}

JpgTranscodingTask::~JpgTranscodingTask() {
	puts("~delete JpgTranscodingTask");
	//releasePileline();
	reportStatus();
	return;
	Packet* newData = new Packet();
	pFlow_->pRawData_in->push(newData);
	Packet* newData1 = new Packet();
	pFlow_->pAudioData_in->push(newData1);
	pFlow_->onComplete(2);
}

void JpgTranscodingTask::reportStatus()
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
void* JpgTranscodingTask::JpgTranscodingTask::startThread(TaskFlow* pFlow)
{
	JpgTranscodingTask* thread = new JpgTranscodingTask(pFlow);

	thread->setAutoDelete(true);
	thread->start();

	return thread;
}
void JpgTranscodingTask::stop()
{
}
bool JpgTranscodingTask::JpgTranscodingTask::onStart()
{
	return true;
}
void JpgTranscodingTask::JpgTranscodingTask::run()
{

	isRun = STATUS_CONNECTING;

	int ret = 0;
	if (pFlow_->inputtype_ == "jpg") {
		ret = buildJpg2JpgPipeline();
	}


	if (ret < 0)
	{
		isRun = STATUS_DISCONNECT;
		DebugLog::writeLogF("jpg build pipe line failed :%s\n", pFlow_->rtspUrl_.c_str());
		{
			closestyle_ = 5;// pipeline create fail
		}
	}
	else
	{
	

		bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
		//gst_bus_add_watch(bus_, EncodingTask::onBusMessage, this);
		gst_element_set_state(pipeline_, GST_STATE_PLAYING);

		do {
			GstMessage* msg = gst_bus_timed_pop_filtered(bus_, 200 * GST_MSECOND, (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

			if (msg != NULL) {
				GError* err;
				gchar* debug_info;

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


// destroy
void JpgTranscodingTask::releasePileline()
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


int JpgTranscodingTask::buildJpg2JpgPipeline() {

	pipeline_ = gst_pipeline_new(std::to_string(tid_).c_str());

	if (!pipeline_) {
		g_printerr("pipeline create fail.\n");
	}
	filesrc_ = gst_element_factory_make("filesrc", ("jpeg_src" + std::to_string(tid_)).c_str());
	fileparse_ = gst_element_factory_make("jpegparse", ("parse0" + std::to_string(tid_)).c_str());

	if (pFlow_->dec_sel_ == 1)
		jpgdecoder_ = gst_element_factory_make("mppjpegdec", ("jpg_decoder0" + std::to_string(tid_)).c_str());
	else
		jpgdecoder_ = gst_element_factory_make("jpegdec", ("jpg_decoder0" + std::to_string(tid_)).c_str());

	if (pFlow_->enc_sel_ == 1) {
		jpgencoder_ = gst_element_factory_make("mppjpegenc", ("jpg_encoder0" + std::to_string(tid_)).c_str());
		g_object_set(G_OBJECT(jpgencoder_), "quant", pFlow_->jpg_out_quant_/10, NULL);

	}
	else {
		jpgencoder_ = gst_element_factory_make("jpegenc", ("jpg_encoder0" + std::to_string(tid_)).c_str());
		g_object_set(G_OBJECT(jpgencoder_), "quality", pFlow_->jpg_out_quant_ , NULL);

	}
	jpgsink_ = gst_element_factory_make("filesink", ("jpeg_sink" + std::to_string(tid_)).c_str());

	if (!pipeline_ | !filesrc_ || !jpgdecoder_ || !jpgencoder_ || !jpgsink_) {
		g_printerr("One element could not be created 2.\n");
		return -1;
	}

	// appsink setting
	g_object_set(G_OBJECT(jpgsink_), "location", pFlow_->outputpath_.c_str(), NULL);
	g_object_set(G_OBJECT(filesrc_), "location", pFlow_->rtspUrl_.c_str(), NULL);
	
	gst_bin_add_many(GST_BIN(pipeline_), filesrc_, fileparse_,jpgdecoder_, jpgencoder_, jpgsink_, NULL);

	if (!gst_element_link_many(filesrc_, fileparse_, jpgdecoder_, jpgencoder_, jpgsink_, NULL)) {
		g_printerr("jpeg Elements could not be linked.\n");
		gst_object_unref(pipeline_);
		return -1;
	}

	return 0;
}
