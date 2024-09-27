#pragma once
#include "RtspTask.h"
#include "RtspTask1.h"

#include "EnhanceTask.h"
#include "EncodingTask.h"
#include "EncodingTask1.h"

#include "RtpTask.h"
#include "../common/ts_json.h"
#include "FilesrcTask.h"
#include "DirectTranscoding.h"
#include "JpgTranscoding.h"


class TaskFlow {
public:
	TaskFlow(TSJson::Value& config);
	virtual ~TaskFlow();

	int start();
	int stop();
	void onComplete(int tasktype);

public:
	FilesrcTask* pFileSrc;
	DirectTranscodingTask* pTrans;
	RtspTask* pRtsp;
	RtpTask* pRtp;
	JpgTranscodingTask* pJpg;
	EnhanceTask* pEnhance;
	EncodingTask* pEncode;
	PacketQueue* pRawData = NULL;
	PacketQueue* pAudioData = NULL;

	PacketQueue* pRawData_in = NULL;
	PacketQueue* pAudioData_in = NULL;

	/*
	* parameters for video smooth
	*/
	int buf_input_=30;
	int buf_decode_ = 5;
	int buf_encode_ = 30;
	int buf_output_ = 30;
	int delay_input_ = 1000;
	int delay_output_ = 1000;

	string encodingStyle_ = "stream";
	string taskConfig_;
	string inputtype_;
	string rtspUrl_;
	string taskid_;
	string outputtype_;
	string encodetype_;
	string outputpath_;
	string vEnhanceMode_="A";
	string aFilterMode_="RD";
	string vfxPath_= "/usr/local/VideoFX/lib/models";
	string afxPath_= "/usr/local/Audio_Effects_SDK/models";
	string gb28181_insdp_;
	string gb28181_outsdp_;
	int autoFramerate_ = 0;
	int bitrate_ = 1024;
	int gop_ = 100;
	int frameRate_ = 25;
	int profile_ = 66;
	string profilestr_ = "high";
	int rcmode_ = 0;

	int jpg_out_quant_ = 50;
	int encwidth_;
	int encheight_;
	int withaudio_ = 0;
	int withfilter_ = 0;
	int haveaudio_ = 0;
	int task_start_time_;
	int complete_children_ = 0;
	int need_callback = 1;

	int src_fs_n_ = 0;
	int src_fs_d_ = 1;
	int targetfps = 0;
	string pixel_fmt_;
	TSJson::Value x264_extra_;
	TSJson::Value x265_extra_;
	TSJson::Value aac_extra_;
	TSJson::Value in_rtp_;
	TSJson::Value out_rtp_;
	ts_mutex cb_lock;
	int queue_size_ = 50;

	int audiochannel = 1;
	int dec_sel_ = -1;
	int enc_sel_ = -1;

	int custom_dev_id_ = 0;

};
