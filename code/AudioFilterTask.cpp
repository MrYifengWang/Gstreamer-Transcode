#include "AudioFilterTask.h"
#include "TaskManager.h"
#include "TaskFlow.h"
#include "../common/ts_time.h"
#include <memory>


AudioFilter::AudioFilter(TaskFlow* pFlow)
{

	pFlow_ = pFlow;
	OutBuf = new float[1024];
	puts("create AudioFilter");
}

AudioFilter::~AudioFilter() {
	puts("~delete AudioFilter");
	if (handle)
		NvAFX_DestroyEffect(handle);

	if (OutBuf)
		delete[] OutBuf;
}
void* AudioFilter::startThread(TaskFlow* pFlow)
{
	AudioFilter* thread = new AudioFilter(pFlow);

	thread->setAutoDelete(true);
	thread->start();
	return thread;
}
bool AudioFilter::onStart()
{
	return true;
}
void AudioFilter::stop()
{
	puts("------stop AudioFilter-----");
}

int AudioFilter::buildFilter()
{
	//NVAFX_EFFECT_DEREVERB_DENOISER
//NvAFX_SetU32(handle, NVAFX_PARAM_ENABLE_VAD, 1);

//status = NvAFX_SetString(handle, NVAFX_PARAM_MODEL_PATH, "/usr/local/Audio_Effects_SDK/models/sm_86/denoiser_16k.trtpgk");
//printf("----audio------------12---	 %d\n", status);

// Chained effect
//	NvAFX_CreateChainedEffect(NVAFX_CHAINED_EFFECT_SUPERRES_8k_TO_16k_DENOISER_16k,&handle);
//	NvAFX_SetStringList(handle, NVAFX_PARAM_MODEL_PATH, model_files,num_model_files);
//	NvAFX_SetU32(handle, NVAFX_PARAM_NUM_STREAMS, 20);

	puts("build audio -------------------------- 0");
	NvAFX_Status status = NvAFX_CreateEffect(NVAFX_EFFECT_DEREVERB_DENOISER, &handle);
	if (status != 0)printf("----audio------------11---	 %d\n", status);

	std::vector<std::string> model_files;
	string itemstr = pFlow_->afxPath_ + "/sm_86/denoiser_16k.trtpkg";
	model_files.push_back(itemstr);
	std::unique_ptr<char*[]> model_files_param(new char*[model_files.size()]);
	for (int i = 0; i < model_files.size(); i++) {
		model_files_param[i] = (char*)model_files[i].data();
	}
	if (NvAFX_SetStringList(handle, NVAFX_PARAM_MODEL_PATH, (const char**)model_files_param.get(), model_files.size()) != NVAFX_STATUS_SUCCESS) {
		if (status != 0)printf("----audio-====================---12---	 %d\n", status);
		return false;
	}
	unsigned int num_streams = 1;
	if (NvAFX_SetU32(handle, NVAFX_PARAM_NUM_STREAMS, num_streams) != NVAFX_STATUS_SUCCESS) {
		if (status != 0)printf("----audio-====================---12---1	 %d\n", status);
		return false;
	}

	puts("build audio -------------------------- 1");

	// Sample rate
	status = NvAFX_SetU32(handle, NVAFX_PARAM_SAMPLE_RATE, input_sample_rate_);
	
	if (status != 0)printf("----audio------------14---	 %d rste=%d\n", status, input_sample_rate_);

	status = NvAFX_SetU32(handle, NVAFX_PARAM_NUM_SAMPLES_PER_INPUT_FRAME, num_input_samples_per_frame_);
	if (status == NVAFX_STATUS_INVALID_PARAM) {
		// Try previous version
		status = NvAFX_SetU32(handle, NVAFX_PARAM_NUM_SAMPLES_PER_FRAME, num_input_samples_per_frame_);
	}

	if (status != 0)printf("----audio------------15---	%d  num=%d\n", status, num_input_samples_per_frame_);

	//// Channels
	status = NvAFX_SetU32(handle, NVAFX_PARAM_NUM_INPUT_CHANNELS, num_input_channels_);
	if (status != 0)printf("----audio------------16---	%d\n", status);

	status = NvAFX_SetFloat(handle, NVAFX_PARAM_INTENSITY_RATIO, 1.0);
	if (status != 0)printf("----audio------------21---	%d\n", status);

	status = NvAFX_SetU32(handle, NVAFX_PARAM_USE_DEFAULT_GPU, 0);
	if (status != 0)printf("----audio------------20---	%d\n", status);

	status = NvAFX_Load(handle);
	if (status != 0)printf("----audio------------13---	%d\n", status);


}
void AudioFilter::run()
{
	//FILE * pcmout = fopen("f32pcm.pcm", "w");

	while (1) {
		Packet* frame = pFlow_->pAudioData_in->trypop(10);
		if (frame != NULL && frame->size > 0) {
			if (!inited_) {
				buildFilter();
				inited_ = true;
			}
			//	For each input sample, process the audio by using NvAFX_Run.
			
			int blocks = frame->size / 1280;
			int mod = frame->size % 1280;
		//	printf("get audio src data---------size=%d block=%d mod=%d", frame->size, blocks, mod);

			int i = 0;
			for (i = 0; i < blocks; i++) {
				input[0] = (float*)frame->pdata_ + i * 320;
				output[0] = OutBuf + i * 320;
				NvAFX_Status status = NvAFX_Run(handle, (const float**)input, (float**)output, num_input_samples_per_frame_, num_input_channels_);
				if (status != 0)
				{
					printf("----audio run ---------------	idx=%d	err =%d size=%d\n", i, status, frame->size);
				}

			}
			if (mod > 0)
			{
				input[0] = (float*)frame->pdata_ + i * 320;
				output[0] = OutBuf + i * 320;
				NvAFX_Status status = NvAFX_Run(handle, (const float**)input, (float**)output, num_input_samples_per_frame_, mod / 4);
				if (status != 0) {
					printf("----audio run mod---------------		err = %d size=%d\n", status, frame->size);

				}
			}
			
			//fwrite(output, frame->size, 1, pcmout);

			if (pFlow_->pAudioData->size() < 50) {
				Packet* newItem = new Packet(output, frame->size);
				newItem->size = frame->size;
				newItem->seq_ = frame->seq_;
				newItem->type_ = frame->type_;
				newItem->pts_ = frame->pts_;
				pFlow_->pAudioData->push(newItem);

			}
			delete frame;
		}
		else {
			Packet* newItem;
			pFlow_->pAudioData->push(newItem);
			puts("audio filter task complete!");
			//	fclose(pcmout);
			return;
		}
	}

	//	fclose(pcmout);

}
