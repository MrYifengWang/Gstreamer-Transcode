#include "TaskFlow.h"
#include "../common/ts_time.h"
#include "TaskManager.h"
#include "DebugLog.h"
#include "../common/EdUrlParser.h"
#include <stdlib.h>

TaskFlow::TaskFlow(TSJson::Value& config)
{
	inputtype_ = "rtsp";
	if (config["rtspUrl"].isString())
		rtspUrl_ = config["rtspUrl"].asString();
	if (config["inputUri"].isString())
		rtspUrl_ = config["inputUri"].asString();
//	rtspUrl_ = EdUrlParser::urlEncode(rtspUrl_);
//	printf("after encode == %s \n", rtspUrl_.c_str());
	if (config["inputType"].isString())
		inputtype_ = config["inputType"].asString();
	if (config["taskID"].isString())
		taskid_ = config["taskID"].asString();
	if (config["outputType"].isString())
		outputtype_ = config["outputType"].asString();
	if (config["encodingStyle"].isString())
		encodingStyle_ = config["encodingStyle"].asString();
	
	if (config["frame_smooth"].isInt())
		autoFramerate_ = config["frame_smooth"].asInt();
	 

	if (config["videoFilter"].isString())
		vEnhanceMode_ = config["videoFilter"].asString();

	if (config["audioFilter"].isString())
		aFilterMode_ = config["audioFilter"].asString();

	if (config["pic_out_quant"].isInt())
		jpg_out_quant_ = config["pic_out_quant"].asInt();

	if (config["encodeType"].isString())
		encodetype_ = config["encodeType"].asString(); // 265 264

	if (config["smoothbuffer"].isString()) {
		string tmpstr = config["smoothbuffer"].asString(); 
		std::vector<std::string> list;
		EdUrlParser::split(tmpstr, ":", &list);
		if (list.size() == 4) {
			 buf_input_ = atoi(list[0].c_str());
			 buf_decode_ = atoi(list[1].c_str()) ;
			 buf_encode_ = atoi(list[2].c_str());
			 buf_output_ = atoi(list[3].c_str());

			 delay_input_ = buf_input_ / 20.0 * 1000;
			 delay_output_ = buf_output_ / 20.0 * 1000;
			 if (delay_input_ < 1000)delay_input_ = 1000;
			 if (delay_output_ < 1000)delay_output_ = 1000;
		}
	}

	if (config["hardwareType"].isString()) {
		puts("--------read start\n");
		string tmp = config["hardwareType"].asString();
		puts("--------read end\n");
		if (tmp == "gg" || tmp == "cg") {
			enc_sel_ = 1;
			printf("we will try to encode with gpu\n");
		}
		else if (tmp == "cc" || tmp == "gc") {
			enc_sel_ = 0;
			printf("we will try to encode with cpu\n");
		}
		if (tmp == "gg" || tmp == "gc") {
			dec_sel_ = 1;
			printf("we will try to decode with gpu\n");
		}
		else if (tmp == "cc" || tmp == "cg") {
			dec_sel_ = 0;
			printf("we will try to decode with cpu\n");
		}
	}
	if (outputtype_ == "flv" || outputtype_ == "rtmp") {
		if (encodetype_ == "h265") {
			encodetype_ = "h264";
		}
	}
	if (config["vfxpath"].isString())
		vfxPath_ = config["vfxpath"].asString();
	if (config["afxpath"].isString())
		afxPath_ = config["afxpath"].asString();

	if (config["outputUri"].isString())
		outputpath_ = config["outputUri"].asString();
	if (config["bitRate"].isInt())
		bitrate_ = config["bitRate"].asInt();
	if (config["withaudio"].isInt())
		withaudio_ = config["withaudio"].asInt();
	if (config["custom_dev"].isInt())
		custom_dev_id_ = config["custom_dev"].asInt();

	if (config["withfilter"].isInt())
		withfilter_ = config["withfilter"].asInt();
	if (outputtype_ == "rtmp") {
		withaudio_ = 0;
	}
	if (config["gop"].isInt())
		gop_ = config["gop"].asInt();
	if (config["bufferSize"].isInt())
		queue_size_ = config["bufferSize"].asInt();
	if (config["profile"].isString()) {
		string pfname = config["profile"].asString();
		profilestr_ = pfname;
		if (pfname == "baseline")
			profile_ = 66;
		else if (pfname == "main")
			profile_ = 66;
		else if (pfname == "high")
			profile_ = 100;
	}
	if (config["in_gb_rtp"].isObject()) {
		in_rtp_ = config["in_gb_rtp"];
	}
	if (config["out_gb_rtp"].isObject()) {
		out_rtp_ = config["out_gb_rtp"];
	}
	if (config["x264_extra"].isObject()) {
		x264_extra_ = config["x264_extra"];
	}
	if (config["x265_extra"].isObject()) {
		x265_extra_ = config["x265_extra"];
	}
	if (config["aac_extra"].isObject()) {
		aac_extra_ = config["aac_extra"];
	}

	if (config["rcmode"].isString()) {
		string pfname = config["rcmode"].asString();
		if (pfname == "vbr")
			rcmode_ = 0;
		else if (pfname == "cbr")
			rcmode_ = 1;
	}

	/*if (bitrate_ < 512)bitrate_ = 512;
	if (bitrate_ > 2048)bitrate_ = 2048;*/
	task_start_time_ = time(NULL);
	haveaudio_ = 0;

	DebugLog::writeLogF("create TaskFlow\n");

}


TaskFlow::~TaskFlow() {
	DebugLog::writeLogF("~delete TaskFlow\n");

	if (withfilter_ == 1) {
		if (pRawData != NULL)
		{
			delete pRawData;
		}
		if (pAudioData != NULL)
		{
			delete pAudioData;
		}
	}
	if (pRawData_in != NULL)
	{
		delete pRawData_in;
		pRawData_in = NULL;
	}
	if (pAudioData_in != NULL)
	{
		delete pAudioData_in;
		pAudioData_in = NULL;
	}

	TaskManager::GetInstance()->removeTask(taskid_);
}

int TaskFlow::start() {
	printf("----------------withaudio =%d  withfilter =%d\n", withaudio_, withfilter_);

	pAudioData_in = new PacketQueue();
	pRawData_in = new PacketQueue();

	if (withfilter_ == 0) {
		pAudioData = pAudioData_in;
		pRawData = pRawData_in;
	}
	else {
		pAudioData = new PacketQueue();
		pRawData = new PacketQueue();
	}
	if (inputtype_ == "rtp") {
		pRtp = (RtpTask*)RtpTask::startThread(this);
	}
	else if (inputtype_ == "mp4" || inputtype_ == "ts" || inputtype_ == "raw") {
		pFileSrc = (FilesrcTask*)FilesrcTask::startThread(this);
		if(encodingStyle_ == "file")
		return 0;
	}else if (inputtype_ == "jpg") {
		pJpg = (JpgTranscodingTask*)JpgTranscodingTask::startThread(this);
		return 0;
	}
	else
	{
		if (rtspUrl_.find("realtime") != std::string::npos && rtspUrl_.find("agent=onvif") != std::string::npos) {
			std::vector<std::string> list;
			EdUrlParser::split(rtspUrl_,";",&list);
			if (list.size() > 1); {
				rtspUrl_ = list[0];
			}

			for (int i = 0; i < list.size(); i++) {
				puts(list[i].c_str());
			}

		}
		if (rtspUrl_.find("LiveMedia") != std::string::npos)// || rtspUrl_.find("realtime") != std::string::npos))// || rtspUrl_.find("realmonitor") != std::string::npos)
		{
			pTrans = (DirectTranscodingTask*)DirectTranscodingTask::startThread(this);
			return 0;
		}
		else
		{
			if (autoFramerate_ == 1) {
				pRtsp = (RtspTask*)RtspTask1::startThread(this);

			}
			else
				pRtsp = (RtspTask*)RtspTask::startThread(this);
		}
		//return 0;
	}
	if (withfilter_ == 1) {
		pEnhance = (EnhanceTask*)EnhanceTask::startThread(this);
	}
	if (autoFramerate_ == 1) {
		pEncode = (EncodingTask*)EncodingTask1::startThread(this);
	}else
	pEncode = (EncodingTask*)EncodingTask::startThread(this);
}
int TaskFlow::stop() {
	if (inputtype_ == "jpg") {
		printf("----stop jpg task----------");
		pJpg->stop();
		return 0;
	}
	if (inputtype_ == "rtp") {
		printf("----stop rtp task----------");
		pRtp->stop();
		return 0;
	}
	need_callback = 0;
	if (inputtype_ == "raw") {
		pFileSrc->stop();
		if (encodingStyle_ == "file")
			return 0;
	}
	if (inputtype_ == "mp4") {
		pFileSrc->stop();
		if (encodingStyle_ == "file")
		return 0;
	}
	else {
		if (rtspUrl_.find("LiveMedia") != std::string::npos)
		{
			pTrans->stop();
			return 0;
		}
		else {
			if (autoFramerate_ == 1) {
				((RtspTask1*)pRtsp)->stop();
			}else
			pRtsp->stop();
		}
		//return 0;
	}
	if (autoFramerate_ == 1) {
		((EncodingTask1*)pEncode)->stop();
	}
	else
	pEncode->stop();
	ts_time::wait(300);
	return 0;
}

void TaskFlow::onComplete(int tasktype)
{

	ts_autoLock lock(cb_lock);
	complete_children_++;
	printf("-----------------------------onComplete- %d %d %d\n", tasktype, complete_children_, need_callback);
	if (complete_children_ >= 2) {
		delete this;
	}
	else {
		if (need_callback > 0) {
			if (tasktype == 1) {
				if (autoFramerate_ == 1) {
					((RtspTask1*)pRtsp)->stop();
				}else
				pRtsp->stop();
			}
			else {
				if (autoFramerate_ == 1) {
					((EncodingTask1*)pEncode)->stop();
				}
				else
				pEncode->stop();
			}
		}
	}
}