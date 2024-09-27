#pragma once
#include "GstTask.h"
#include "../common/ts_thread.h"
#include "PacketQueue.h"
#include <string>
#include <nvAudioEffects.h>

using namespace std;

class TaskFlow;
class RtspTask;
class StatusQueue;
class AudioFilter : public ts_thread
{
	AudioFilter(TaskFlow* pFlow);
	virtual ~AudioFilter();
public:
	static void* startThread(TaskFlow* pFlow);
	void stop();
protected:
	virtual bool onStart();
	virtual void run();

private:
	int buildFilter();


private:
	bool inited_ = false;
	TaskFlow* pFlow_;
	//string model1 = "../../models/sm_86/denoiser_16k.trtpkg";
	string model1 = "/usr/local/Audio_Effects_SDK/models/sm_86/denoiser_16k.trtpkg";
	NvAFX_Handle handle;
	unsigned int input_sample_rate_ = 16000;
	unsigned int output_sample_rate_ = 16000;
	unsigned int num_input_channels_ =1;
	unsigned int num_output_channels_ =1;
	unsigned int num_input_samples_per_frame_=320;
	unsigned int num_output_samples_per_frame_=320;
	float* input[2];
	float* output[2];
	float* OutBuf=NULL;

};